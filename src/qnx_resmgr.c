/**
 * @file qnx_resmgr.c
 * @brief Exposes /dev/qnx_top as a native QNX 8.0 Resource Manager
 * 
 * Features:
 * - Dedicated POSIX dispatch thread loop (dispatch_block -> dispatch_handler)
 * - Strict POSIX read semantics with EOF handling on ocb->offset
 * - Dynamic heap buffer allocation (safe from stack overflow)
 * - Secure permissions (0644)
 * - Clean asynchronous thread termination
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
    #include <pthread.h>
#endif

#ifdef __QNX__
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>

static dispatch_t           *g_dpp = NULL;
static resmgr_connect_funcs_t g_connect_funcs;
static resmgr_io_funcs_t      g_io_funcs;
static iofunc_attr_t          g_attr;
static pthread_t              g_resmgr_tid = 0;
static volatile bool          g_resmgr_running = false;
static int                    g_resmgr_id = -1;

/**
 * Custom POSIX read handler for /dev/qnx_top
 */
static int io_read_handler(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    /* If the reader has already read past the start, return EOF (0 bytes) */
    if (ocb->offset > 0) {
        _IO_SET_READ_NBYTES(ctp, 0);
        return _RESMGR_PTR(ctp, NULL, 0);
    }

    /* Allocate telemetry snapshot dynamically on heap */
    qnx_system_snapshot_t *snapshot = malloc(sizeof(qnx_system_snapshot_t));
    if (!snapshot) {
        return ENOMEM;
    }

    qnx_top_collect_snapshot(snapshot);

    /* Allocate formatting buffer dynamically on heap */
    size_t buf_capacity = 8192;
    char *buffer = malloc(buf_capacity);
    if (!buffer) {
        free(snapshot);
        return ENOMEM;
    }

    int len = snprintf(buffer, buf_capacity,
        "======================================================================\n"
        " QNX-TOP v%s Microkernel Diagnostic Endpoint [/dev/qnx_top]\n"
        " Mode: %s\n"
        "======================================================================\n"
        "SMP Cores: %u | RAM Used: %lu MB / %lu MB | ResMgr Heap: %lu KB\n"
        "Total Processes: %d | Active Channels: %d | Partitions: %d\n"
        "Total IPC Msg Rate: %lu msgs/sec\n\n"
        "Top Active Processes:\n",
        QNX_TOP_VERSION,
        snapshot->is_native_qnx ? "QNX Neutrino 8.0 Native" : "Host Simulated",
        snapshot->num_cores,
        (unsigned long)snapshot->ram_used_mb,
        (unsigned long)snapshot->ram_total_mb,
        (unsigned long)snapshot->resmgr_pool_kb,
        snapshot->num_processes,
        snapshot->num_channels,
        snapshot->num_partitions,
        (unsigned long)snapshot->total_channel_msgs_sec
    );

    /* Append summary of top processes */
    for (int i = 0; i < snapshot->num_processes && i < 10; i++) {
        qnx_process_info_t *p = &snapshot->processes[i];
        len += snprintf(buffer + len, buf_capacity - len,
            "  PID %-6d PRI %-3d State: %-15s CPU: %4.1f%%  %s\n",
            p->pid, p->threads[0].priority,
            qnx_thread_state_to_string(p->threads[0].state),
            p->cpu_percent, p->name);
    }

    free(snapshot);

    int nbytes = msg->i.nbytes;
    if (len > nbytes) {
        len = nbytes;
    }

    /* Advance the offset so subsequent read() calls receive EOF */
    ocb->offset += len;

    _IO_SET_READ_NBYTES(ctp, len);
    int status = resmgr_msgwrite(ctp, buffer, len, 0);

    free(buffer);
    return status;
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
        ctp = dispatch_block(ctp);
        if (!ctp) {
            break;
        }
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

    /* Tighten permissions to standard 0644 (Read by all, write by root only) */
    iofunc_attr_init(&g_attr, S_IFCHR | 0644, 0, 0);

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
        resmgr_detach(g_dpp, g_resmgr_id, _RESMGR_DETACH_ALL);
        dispatch_destroy(g_dpp);
        g_dpp = NULL;
        return -1;
    }

    printf("[qnx-top] Resource Manager online at /dev/qnx_top (dispatch thread ID: %lu, perms: 0644)\n",
           (unsigned long)g_resmgr_tid);
    return 0;
}

void qnx_top_stop_resmgr(void) {
    if (g_resmgr_running) {
        g_resmgr_running = false;
        if (g_resmgr_id != -1 && g_dpp) {
            resmgr_detach(g_dpp, g_resmgr_id, _RESMGR_DETACH_ALL);
            g_resmgr_id = -1;
        }
        if (g_resmgr_tid != 0) {
            pthread_cancel(g_resmgr_tid);
            pthread_join(g_resmgr_tid, NULL);
            g_resmgr_tid = 0;
        }
        if (g_dpp) {
            dispatch_destroy(g_dpp);
            g_dpp = NULL;
        }
        printf("[qnx-top] Resource Manager /dev/qnx_top detached cleanly.\n");
    }
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
