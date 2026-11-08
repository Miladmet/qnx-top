/**
 * @file main.c
 * @brief Interactive CLI profiler and diagnostic monitor for QNX 8.0
 *
 * Any metric the platform cannot measure is printed as "N/A".
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define sleep_ms(ms) Sleep(ms)
#else
    #include <unistd.h>
    #define sleep_ms(ms) do { \
        struct timespec ts = { (time_t)((ms)/1000), (long)(((ms)%1000)*1000000L) }; \
        nanosleep(&ts, NULL); \
    } while(0)
#endif

static volatile sig_atomic_t g_running = 1;

static void handle_sigint(int sig) {
    (void)sig;
    g_running = 0;
}

typedef struct {
    bool all_threads;   /* -t: one row per thread instead of one row per process */
    int pid_filter;     /* -p: only this process (implies thread rows); 0 = all */
} view_options_t;

static void print_usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  -d <seconds>   Refresh delay in seconds (default: 1.0, minimum 0.1)\n");
    printf("  -n <count>     Exit after <count> refreshes (default: run until interrupted)\n");
    printf("  -b             Batch mode (single snapshot, no screen clear)\n");
    printf("  -t             Thread view: one row per thread\n");
    printf("  -p <pid>       Show only this process, with its threads\n");
    printf("  -h             Show this help message\n");
}

static bool parse_int(const char *text, int min, int max, int *out) {
    char *end = NULL;
    errno = 0;
    long v = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || v < min || v > max) return false;
    *out = (int)v;
    return true;
}

static bool parse_float(const char *text, float min, float max, float *out) {
    char *end = NULL;
    errno = 0;
    double v = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || v < min || v > max) return false;
    *out = (float)v;
    return true;
}

/* Formatting helpers: return "N/A" text when a metric is not measured. */
static const char *fmt_pct(char *buf, size_t n, bool available, float value) {
    if (!available) return "N/A";
    snprintf(buf, n, "%.1f%%", value);
    return buf;
}

static const char *fmt_u64(char *buf, size_t n, bool available, uint64_t value) {
    if (!available) return "N/A";
    snprintf(buf, n, "%lu", (unsigned long)value);
    return buf;
}

static const char *fmt_bytes(char *buf, size_t n, bool available, uint64_t value) {
    if (!available) return "N/A";
    return qnx_format_bytes(buf, n, value);
}

/*
 * REPLY-block duration with its source. Kernel values are exact (ms resolution).
 * Sampled values are a lower bound: the time since qnx-top first saw the block.
 */
static const char *fmt_stall(char *buf, size_t n, float ms, qnx_stall_source_t source) {
    if (source == QNX_STALL_KERNEL) {
        snprintf(buf, n, "%.1fms (kernel)", ms);
    } else if (source == QNX_STALL_SAMPLED) {
        snprintf(buf, n, ">=%.1fms (sampled)", ms);
    } else {
        snprintf(buf, n, "-");
    }
    return buf;
}

/* Prefer the REPLY-blocked thread so stalls are visible in the process table. */
static const qnx_thread_info_t *pick_thread(const qnx_process_info_t *p) {
    for (int t = 0; t < p->num_threads; t++) {
        if (p->threads[t].state == QNX_THREAD_REPLY_BLOCKED) return &p->threads[t];
    }
    return p->num_threads > 0 ? &p->threads[0] : NULL;
}

static void fmt_chid(char *buf, size_t n, int chid) {
    if (chid > 0) snprintf(buf, n, "0x%02x", (unsigned)chid);
    else snprintf(buf, n, "-");
}

static void print_process_rows(const qnx_system_snapshot_t *snapshot) {
    char a[64], b[64], c[64], d[96];

    printf("PID    TID  PRI   STATE             CPU%%     RSS      CHID   PULSES     COMMAND\n");
    printf("------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < snapshot->num_processes; i++) {
        const qnx_process_info_t *p = &snapshot->processes[i];
        const qnx_thread_info_t *t = pick_thread(p);
        if (!t) continue;

        char chid_buf[16];
        fmt_chid(chid_buf, sizeof(chid_buf), p->active_chid);

        printf("%-6d %-4d %-5d %-17s %-8s %-8s %-6s %-10s %-20s",
               p->pid, t->tid, t->priority, qnx_thread_state_to_string(t->state),
               fmt_pct(a, sizeof(a), p->cpu_valid, p->cpu_percent),
               fmt_bytes(b, sizeof(b), p->rss_valid, p->rss_bytes),
               chid_buf,
               fmt_u64(c, sizeof(c), snapshot->avail.pulses, p->total_pulses_received),
               p->name);
        if (t->state == QNX_THREAD_REPLY_BLOCKED) {
            printf(" [waiting on PID %d CHID 0x%02x: %s]",
                   t->blocked_on_pid, (unsigned)t->blocked_on_chid,
                   fmt_stall(d, sizeof(d), t->reply_stall_ms, t->stall_source));
        }
        printf("\n");
    }
}

static void print_thread_rows(const qnx_system_snapshot_t *snapshot, int pid_filter) {
    char a[64], b[64], c[64], d[96];
    int shown_processes = 0;

    for (int i = 0; i < snapshot->num_processes; i++) {
        const qnx_process_info_t *p = &snapshot->processes[i];
        if (pid_filter > 0 && p->pid != pid_filter) continue;
        shown_processes++;

        printf("PID %d  %s  (PPID %d)  VSIZE %s  RSS %s  CPU %s  threads %d\n",
               p->pid, p->name, p->ppid,
               fmt_bytes(a, sizeof(a), true, p->memory_vsize_bytes),
               fmt_bytes(b, sizeof(b), p->rss_valid, p->rss_bytes),
               fmt_pct(c, sizeof(c), p->cpu_valid, p->cpu_percent),
               p->num_threads_total > 0 ? p->num_threads_total : p->num_threads);
        printf("  %-4s %-4s %-9s %-14s %-8s %-4s %s\n",
               "TID", "PRI", "POLICY", "STATE", "CPU%", "CPU#", "BLOCKED ON");

        for (int t = 0; t < p->num_threads; t++) {
            const qnx_thread_info_t *th = &p->threads[t];
            char cpu_no[16];
            if (th->last_cpu >= 0) snprintf(cpu_no, sizeof(cpu_no), "%d", th->last_cpu);
            else snprintf(cpu_no, sizeof(cpu_no), "N/A");

            printf("  %-4d %-4d %-9s %-14s %-8s %-4s ",
                   th->tid, th->priority, qnx_policy_to_string(th->policy),
                   qnx_thread_state_to_string(th->state),
                   fmt_pct(a, sizeof(a), th->cpu_valid, th->cpu_percent), cpu_no);

            if (th->state == QNX_THREAD_REPLY_BLOCKED) {
                printf("server PID %d CHID 0x%02x, waiting %s",
                       th->blocked_on_pid, (unsigned)th->blocked_on_chid,
                       fmt_stall(d, sizeof(d), th->reply_stall_ms, th->stall_source));
            } else {
                printf("-");
            }
            printf("\n");
        }
        if (p->num_threads_total > p->num_threads) {
            printf("  (+%d more threads not shown; limit is %d per process)\n",
                   p->num_threads_total - p->num_threads, MAX_THREADS_PER_PROC);
        }
        printf("\n");
    }

    if (shown_processes == 0) {
        if (pid_filter > 0) printf("No process with PID %d was found.\n\n", pid_filter);
        else printf("No processes found.\n\n");
    }
}

static void print_snapshot(const qnx_system_snapshot_t *snapshot, const view_options_t *view) {
    char a[64], b[64], c[64], d[64];

    printf("==========================================================================================\n");
    printf(" QNX-TOP v%s | %s | Cores: %u\n",
           QNX_TOP_VERSION,
           snapshot->is_native_qnx ? "QNX NEUTRINO 8.0 [LIVE KERNEL]" : "HOST MODE [SIMULATED TELEMETRY]",
           snapshot->num_cores);
    printf("==========================================================================================\n");

    /* Per-core metrics */
    if (!snapshot->avail.core_util && !snapshot->avail.core_ctxsw) {
        printf("Per-core CPU%% / CtxSw: N/A (requires TraceEvent instrumentation)\n");
    } else {
        for (uint32_t i = 0; i < snapshot->num_cores && i < MAX_CORES; i++) {
            printf("Core %u: [%s] (CtxSw: %s/s)%s", i,
                   fmt_pct(a, sizeof(a), snapshot->avail.core_util, snapshot->core_utilization[i]),
                   fmt_u64(b, sizeof(b), snapshot->avail.core_ctxsw, snapshot->core_ctxsw_per_sec[i]),
                   (i % 2 == 1 || i + 1 == snapshot->num_cores) ? "\n" : "   |   ");
        }
    }

    /* Memory */
    const char *ram_used = fmt_u64(a, sizeof(a), snapshot->avail.ram_used, snapshot->ram_used_mb);
    const char *ram_total = fmt_u64(b, sizeof(b), snapshot->avail.ram_total, snapshot->ram_total_mb);
    const char *pool = fmt_u64(c, sizeof(c), snapshot->avail.pools, snapshot->resmgr_pool_kb);
    const char *msgs = fmt_u64(d, sizeof(d), snapshot->avail.msg_rates, snapshot->total_channel_msgs_sec);
    printf("RAM: %s MB / %s MB", ram_used, ram_total);
    if (snapshot->avail.ram_used && snapshot->avail.ram_total && snapshot->ram_total_mb > 0) {
        printf(" (%4.1f%%)", ((float)snapshot->ram_used_mb / (float)snapshot->ram_total_mb) * 100.0f);
    }
    printf(" | ResMgr Pool: %s KB | Total Msg Rate: %s msgs/sec\n", pool, msgs);
    printf("REPLY-block time source: %s\n",
           snapshot->avail.block_time_kernel
               ? "kernel (nsec_since_block), sampled lower bound as fallback"
               : "sampled lower bound (kernel field not available in this build)");
    printf("------------------------------------------------------------------------------------------\n");

    if (view->all_threads || view->pid_filter > 0) {
        print_thread_rows(snapshot, view->pid_filter);
    } else {
        print_process_rows(snapshot);
    }

    /* Channels with REPLY-blocked clients (grouped by SERVER channel) */
    printf("\n[Server Channels With REPLY-Blocked Clients]\n");
    printf("%-8s %-10s %-22s %-16s %-14s %-10s %-26s\n",
           "CHID", "SERVER PID", "SERVER", "RECEIVE THREAD", "BLOCKED CLIENTS", "MSG/SEC", "STATUS");
    printf("------------------------------------------------------------------------------------------\n");

    if (snapshot->num_channels == 0) {
        printf("(no client threads are currently REPLY-blocked)\n");
    }
    for (int i = 0; i < snapshot->num_channels; i++) {
        const qnx_channel_info_t *ch = &snapshot->channels[i];
        char rx[64];
        if (ch->receive_tid >= 0) snprintf(rx, sizeof(rx), "TID %d (PRI %d)", ch->receive_tid, ch->receive_priority);
        else snprintf(rx, sizeof(rx), "N/A");

        char stall[96];
        char status[160];
        fmt_stall(stall, sizeof(stall), ch->max_stall_latency_ms, ch->stall_source);
        if (ch->blocked_clients == 0) snprintf(status, sizeof(status), "no blocked clients");
        else snprintf(status, sizeof(status), "%s %s", ch->is_stalled ? "STALL" : "OK", stall);

        printf("0x%02x     %-10d %-22s %-16s %-14d %-10s %-26s\n",
               (unsigned)ch->chid, ch->owner_pid, ch->owner_name, rx, ch->blocked_clients,
               fmt_u64(a, sizeof(a), snapshot->avail.msg_rates, ch->messages_per_sec), status);
    }

    /* Adaptive Partitioning System */
    printf("\n[Adaptive Partitioning System (APS)]\n");
    if (snapshot->num_partitions > 0) {
        for (int i = 0; i < snapshot->num_partitions; i++) {
            const qnx_aps_partition_t *pt = &snapshot->partitions[i];
            printf("* %-22s Budget: %4.1f%% | Current: %4.1f%% | Max Jitter: <%lu us %s\n",
                   pt->name, pt->budget_percent, pt->current_usage_percent,
                   (unsigned long)pt->max_jitter_us, pt->is_critical ? "[HARD REAL-TIME]" : "");
        }
    } else if (snapshot->aps_active) {
        printf("Adaptive Partitioning: Active (per-partition budgets N/A)\n");
    } else {
        printf("Adaptive Partitioning: Inactive / Not Configured\n");
    }

    /* /dev entries */
    printf("\n[/dev Entries]\n");
    for (int i = 0; i < snapshot->num_dev_nodes; i++) {
        const qnx_resmgr_node_t *n = &snapshot->dev_nodes[i];
        printf("* %-24s Owner: %-18s Handlers: %-25s (Req/s: %s)\n",
               n->path,
               snapshot->avail.dev_details ? n->owner_name : "N/A",
               snapshot->avail.dev_details ? n->handlers : "N/A",
               fmt_u64(a, sizeof(a), snapshot->avail.dev_details, n->io_requests_per_sec));
    }
}

int main(int argc, char *argv[]) {
    float delay_sec = 1.0f;
    int max_iterations = 0;     /* 0 = run until interrupted */
    bool batch_mode = false;
    view_options_t view = { false, 0 };

    /* Parse CLI arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            if (!parse_float(argv[++i], 0.1f, 3600.0f, &delay_sec)) {
                fprintf(stderr, "%s: invalid delay '%s' (expected 0.1 to 3600)\n", argv[0], argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            if (!parse_int(argv[++i], 1, 1000000000, &max_iterations)) {
                fprintf(stderr, "%s: invalid count '%s' (expected a positive integer)\n", argv[0], argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            if (!parse_int(argv[++i], 1, 2147483647, &view.pid_filter)) {
                fprintf(stderr, "%s: invalid pid '%s'\n", argv[0], argv[i]);
                return 2;
            }
        } else if (strcmp(argv[i], "-t") == 0) {
            view.all_threads = true;
        } else if (strcmp(argv[i], "-b") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "%s: unknown or incomplete option '%s'\n\n", argv[0], argv[i]);
            print_usage(argv[0]);
            return 2;
        }
    }

    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    if (qnx_top_init() != 0) {
        fprintf(stderr, "[qnx-top] Initialization failed.\n");
        return 1;
    }

    /* Start the native resource manager /dev/qnx_top */
    qnx_top_start_resmgr();

    /* The snapshot is several hundred KB: keep it off the stack. */
    qnx_system_snapshot_t *snapshot = malloc(sizeof(*snapshot));
    if (!snapshot) {
        fprintf(stderr, "[qnx-top] Out of memory.\n");
        qnx_top_stop_resmgr();
        return 1;
    }

    int iteration = 0;
    while (g_running) {
        if (qnx_top_collect_snapshot(snapshot) != 0) {
            fprintf(stderr, "[qnx-top] Failed to collect system snapshot.\n");
            break;
        }

        if (!batch_mode) {
            /* ANSI Cursor home and clear from cursor to end of screen */
            printf("\033[H\033[J");
        }

        print_snapshot(snapshot, &view);

        iteration++;
        if ((max_iterations > 0 && iteration >= max_iterations) || batch_mode) {
            break;
        }

        sleep_ms((int)(delay_sec * 1000.0f));
    }

    free(snapshot);
    printf("\n[qnx-top] Stopping diagnostic monitor...\n");
    qnx_top_stop_resmgr();
    qnx_top_cleanup();
    return 0;
}
