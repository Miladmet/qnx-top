/**
 * @file qnx_resmgr.c
 * @brief Exposes /dev/qnx_top as a native QNX 8.0 Resource Manager
 *
 * Features:
 * - Dedicated POSIX dispatch thread loop (dispatch_block -> dispatch_handler)
 * - Strict POSIX read semantics with EOF handling on ocb->offset
 * - Dynamic heap buffer allocation (safe from stack overflow)
 * - Permissions 0644
 * - Reports only measured metrics; unmeasured ones are printed as "N/A"
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>

#if defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
    #include <pthread.h>
#endif

#ifdef __QNX__
#include <sys/stat.h>
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>

#define REPORT_CAPACITY 8192

static dispatch_t           *g_dpp = NULL;
static resmgr_connect_funcs_t g_connect_funcs;
static resmgr_io_funcs_t      g_io_funcs;
static iofunc_attr_t          g_attr;
static pthread_t              g_resmgr_tid;
static bool                   g_resmgr_thread_started = false;
static volatile bool          g_resmgr_running = false;
static int                    g_resmgr_id = -1;

/* Bounded printf-append: never writes past capacity, never lets len overflow. */
static void report_append(char *buf, size_t capacity, size_t *len, const char *fmt, ...) {
    if (*len >= capacity) return;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf + *len, capacity - *len, fmt, ap);
    va_end(ap);
    if (n < 0) return;
    if ((size_t)n >= capacity - *len) {
        *len = capacity - 1;   /* truncated */
    } else {
        *len += (size_t)n;
    }
}

static void format_report(const qnx_system_snapshot_t *s, char *buf, size_t capacity, size_t *len) {
    report_append(buf, capacity, len,
        "======================================================================\n"
        " QNX-TOP v%s Microkernel Diagnostic Endpoint [/dev/qnx_top]\n"
        " Mode: %s\n"
        "======================================================================\n"
        "SMP Cores: %u\n",
        QNX_TOP_VERSION,
        s->is_native_qnx ? "QNX Neutrino 8.0 Native" : "Host Simulated",
        s->num_cores);

    if (s->avail.ram_total && s->avail.ram_used) {
        report_append(buf, capacity, len, "RAM: %lu MB used / %lu MB total\n",
                      (unsigned long)s->ram_used_mb, (unsigned long)s->ram_total_mb);
    } else if (s->avail.ram_total) {
        report_append(buf, capacity, len, "RAM: N/A used / %lu MB total\n",
                      (unsigned long)s->ram_total_mb);
    } else {
        report_append(buf, capacity, len, "RAM: N/A\n");
    }

    report_append(buf, capacity, len, "Processes: %d | Channels with blocked clients: %d\n",
                  s->num_processes, s->num_channels);

    if (s->num_partitions > 0) {
        report_append(buf, capacity, len, "Adaptive Partitioning: %d partitions\n", s->num_partitions);
    } else if (s->aps_active) {
        report_append(buf, capacity, len, "Adaptive Partitioning: Active (per-partition metrics N/A)\n");
    } else {
        report_append(buf, capacity, len, "Adaptive Partitioning: Inactive / Not Configured\n");
    }

    report_append(buf, capacity, len, "\nProcesses:\n");
    for (int i = 0; i < s->num_processes && i < 10; i++) {
        const qnx_process_info_t *p = &s->processes[i];
        qnx_thread_state_t st = p->num_threads > 0 ? p->threads[0].state : QNX_THREAD_UNKNOWN;
        int pri = p->num_threads > 0 ? p->threads[0].priority : 0;
        if (p->cpu_valid) {
            report_append(buf, capacity, len, "  PID %-6d PRI %-3d State: %-15s CPU: %4.1f%%  %s\n",
                          p->pid, pri, qnx_thread_state_to_string(st), p->cpu_percent, p->name);
        } else {
            report_append(buf, capacity, len, "  PID %-6d PRI %-3d State: %-15s CPU: N/A    %s\n",
                          p->pid, pri, qnx_thread_state_to_string(st), p->name);
        }
    }
}

/**
 * Custom POSIX read handler for /dev/qnx_top
 */
static int io_read_handler(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    int status = iofunc_read_verify(ctp, msg, ocb, NULL);
    if (status != EOK) {
        return status;
    }
    if ((msg->i.xtype & _IO_XTYPE_MASK) != _IO_XTYPE_NONE) {
        return ENOSYS;
    }

    /* If the reader has already read past the start, return EOF (0 bytes) */
    if (ocb->offset > 0) {
        _IO_SET_READ_NBYTES(ctp, 0);
        return _RESMGR_NPARTS(0);
    }

    /* Allocate telemetry snapshot and report buffer on the heap */
    qnx_system_snapshot_t *snapshot = malloc(sizeof(*snapshot));
    char *buffer = malloc(REPORT_CAPACITY);
    if (!snapshot || !buffer) {
        free(snapshot);
        free(buffer);
        return ENOMEM;
    }

    if (qnx_top_collect_snapshot(snapshot) != 0) {
        free(snapshot);
        free(buffer);
        return EIO;
    }

    size_t len = 0;
    buffer[0] = '\0';
    format_report(snapshot, buffer, REPORT_CAPACITY, &len);
    free(snapshot);

    size_t nbytes = (size_t)msg->i.nbytes;
    if (len > nbytes) {
        len = nbytes;
    }

    if (len > 0 && resmgr_msgwrite(ctp, buffer, len, 0) == -1) {
        int err = errno;
        free(buffer);
        return err != 0 ? err : EIO;
    }
    free(buffer);

    /* Advance the offset so subsequent read() calls receive EOF */
    ocb->offset += (off_t)len;

    _IO_SET_READ_NBYTES(ctp, (int)len);
    return _RESMGR_NPARTS(0);
}

/**
 * Background worker thread executing the QNX message dispatch loop
 */
static void* resmgr_dispatch_worker(void *arg) {
    (void)arg;
    dispatch_context_t *ctp = dispatch_context_alloc(g_dpp);
    if (!ctp) {
        perror("[qnx-top] dispatch_context_alloc failed");
        return NULL;
    }

    while (g_resmgr_running) {
        dispatch_context_t *next = dispatch_block(ctp);
        if (!next) {
            break;
        }
        ctp = next;
        dispatch_handler(ctp);
    }

    dispatch_context_free(ctp);
    return NULL;
}

int qnx_top_start_resmgr(void) {
    g_dpp = dispatch_create();
    if (!g_dpp) {
        perror("[qnx-top] dispatch_create failed");
        return -1;
    }

    resmgr_attr_t resmgr_attr;
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 1;
    resmgr_attr.msg_max_size = 8192;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &g_connect_funcs,
                     _RESMGR_IO_NFUNCS, &g_io_funcs);
    g_io_funcs.read = io_read_handler;

    /* Permissions 0644 (read by all, write by owner only) */
    iofunc_attr_init(&g_attr, S_IFCHR | 0644, NULL, NULL);

    g_resmgr_id = resmgr_attach(g_dpp, &resmgr_attr, "/dev/qnx_top", _FTYPE_ANY,
                                0, &g_connect_funcs, &g_io_funcs, &g_attr);
    if (g_resmgr_id == -1) {
        perror("[qnx-top] resmgr_attach failed");
        dispatch_destroy(g_dpp);
        g_dpp = NULL;
        return -1;
    }

    /* Spawn dedicated background thread for the dispatch loop */
    g_resmgr_running = true;
    if (pthread_create(&g_resmgr_tid, NULL, resmgr_dispatch_worker, NULL) != 0) {
        perror("[qnx-top] pthread_create for resmgr dispatch loop failed");
        g_resmgr_running = false;
        resmgr_detach(g_dpp, g_resmgr_id, _RESMGR_DETACH_ALL);
        g_resmgr_id = -1;
        dispatch_destroy(g_dpp);
        g_dpp = NULL;
        return -1;
    }
    g_resmgr_thread_started = true;

    printf("[qnx-top] Resource Manager online at /dev/qnx_top (perms: 0644)\n");
    return 0;
}

void qnx_top_stop_resmgr(void) {
    if (!g_resmgr_thread_started) {
        return;
    }
    g_resmgr_running = false;
    if (g_resmgr_id != -1 && g_dpp) {
        resmgr_detach(g_dpp, g_resmgr_id, _RESMGR_DETACH_ALL);
        g_resmgr_id = -1;
    }
    /* Collector disables cancellation while holding its mutex, so this cannot
     * interrupt a snapshot half-way and leave the history lock held. */
    pthread_cancel(g_resmgr_tid);
    pthread_join(g_resmgr_tid, NULL);
    g_resmgr_thread_started = false;
    if (g_dpp) {
        dispatch_destroy(g_dpp);
        g_dpp = NULL;
    }
    printf("[qnx-top] Resource Manager /dev/qnx_top detached cleanly.\n");
}
#else
/* Host stub */
int qnx_top_start_resmgr(void) {
    printf("[qnx-top] [HOST MODE] Resource Manager /dev/qnx_top simulated.\n");
    return 0;
}

void qnx_top_stop_resmgr(void) {
    /* No-op on host */
}
#endif
