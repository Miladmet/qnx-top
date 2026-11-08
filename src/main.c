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

static void print_usage(const char *progname) {
    printf("Usage: %s [options]\n", progname);
    printf("Options:\n");
    printf("  -d <seconds>   Refresh delay in seconds (default: 1.0)\n");
    printf("  -n <count>     Exit after <count> iterations (default: continuous)\n");
    printf("  -b             Batch mode (single snapshot, no screen clear)\n");
    printf("  -h             Show this help message\n");
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

/* Prefer the REPLY-blocked thread so stalls are visible in the process table. */
static const qnx_thread_info_t *pick_thread(const qnx_process_info_t *p) {
    for (int t = 0; t < p->num_threads; t++) {
        if (p->threads[t].state == QNX_THREAD_REPLY_BLOCKED) return &p->threads[t];
    }
    return p->num_threads > 0 ? &p->threads[0] : NULL;
}

static void print_snapshot(const qnx_system_snapshot_t *snapshot) {
    char a[32], b[32], c[32], d[32];

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
    printf("------------------------------------------------------------------------------------------\n");

    /* Process & thread state matrix */
    printf("PID    TID  PRI   STATE             CPU%%     CHID   PULSES     COMMAND\n");
    printf("------------------------------------------------------------------------------------------\n");

    for (int i = 0; i < snapshot->num_processes; i++) {
        const qnx_process_info_t *p = &snapshot->processes[i];
        const qnx_thread_info_t *t = pick_thread(p);
        if (!t) continue;

        char chid_buf[16];
        if (p->active_chid > 0) snprintf(chid_buf, sizeof(chid_buf), "0x%02x", (unsigned)p->active_chid);
        else snprintf(chid_buf, sizeof(chid_buf), "-");

        const char *cpu = fmt_pct(a, sizeof(a), p->cpu_valid, p->cpu_percent);
        const char *pulses = fmt_u64(b, sizeof(b), snapshot->avail.pulses, p->total_pulses_received);

        printf("%-6d %-4d %-5d %-17s %-8s %-6s %-10s %-20s",
               p->pid, t->tid, t->priority, qnx_thread_state_to_string(t->state),
               cpu, chid_buf, pulses, p->name);
        if (t->state == QNX_THREAD_REPLY_BLOCKED) {
            printf(" (waiting %.1fms on PID %d CHID 0x%02x)",
                   t->reply_stall_ms, t->blocked_on_pid, (unsigned)t->blocked_on_chid);
        }
        printf("\n");
    }

    /* Channels with REPLY-blocked clients (grouped by SERVER channel) */
    printf("\n[Server Channels With REPLY-Blocked Clients]\n");
    printf("%-8s %-10s %-22s %-16s %-14s %-10s %-22s\n",
           "CHID", "SERVER PID", "SERVER", "RECEIVE THREAD", "BLOCKED CLIENTS", "MSG/SEC", "STATUS");
    printf("------------------------------------------------------------------------------------------\n");

    if (snapshot->num_channels == 0) {
        printf("(no client threads are currently REPLY-blocked)\n");
    }
    for (int i = 0; i < snapshot->num_channels; i++) {
        const qnx_channel_info_t *ch = &snapshot->channels[i];
        char rx[32];
        if (ch->receive_tid >= 0) snprintf(rx, sizeof(rx), "TID %d (PRI %d)", ch->receive_tid, ch->receive_priority);
        else snprintf(rx, sizeof(rx), "N/A");

        char status[32];
        if (ch->is_stalled) snprintf(status, sizeof(status), "STALL (%.1fms)", ch->max_stall_latency_ms);
        else snprintf(status, sizeof(status), "OK (%.1fms)", ch->max_stall_latency_ms);

        printf("0x%02x     %-10d %-22s %-16s %-14d %-10s %-22s\n",
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
    int max_iterations = -1;
    bool batch_mode = false;

    /* Parse CLI arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay_sec = (float)atof(argv[++i]);
            if (delay_sec < 0.1f) delay_sec = 0.1f;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            max_iterations = atoi(argv[++i]);
        } else if (strcmp(argv[i], "-b") == 0) {
            batch_mode = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
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

        print_snapshot(snapshot);

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
