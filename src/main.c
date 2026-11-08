/**
 * @file main.c
 * @brief Interactive CLI profiler and diagnostic monitor for QNX 8.0
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

int main(int argc, char *argv[]) {
    float delay_sec = 1.0f;
    int max_iterations = -1;
    bool batch_mode = false;

    /* Parse CLI arguments */
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay_sec = atof(argv[++i]);
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

    /* Register signal handler for graceful shutdown */
    signal(SIGINT, handle_sigint);
    signal(SIGTERM, handle_sigint);

    if (qnx_top_init() != 0) {
        fprintf(stderr, "[qnx-top] Initialization failed.\n");
        return 1;
    }

    /* Start the native resource manager /dev/qnx_top */
    qnx_top_start_resmgr();

    int iteration = 0;
    while (g_running) {
        qnx_system_snapshot_t snapshot;
        if (qnx_top_collect_snapshot(&snapshot) != 0) {
            fprintf(stderr, "[qnx-top] Failed to collect system snapshot.\n");
            break;
        }

        if (!batch_mode) {
            /* ANSI Cursor home and clear from cursor to end of screen */
            printf("\033[H\033[J");
        }

        /* 1. Header with clear mode identification */
        printf("==========================================================================================\n");
        printf(" QNX-TOP v%s │ %s │ Cores: %u\n",
               QNX_TOP_VERSION,
               snapshot.is_native_qnx ? "QNX NEUTRINO 8.0 [LIVE KERNEL]" : "HOST MODE [SIMULATED TELEMETRY]",
               snapshot.num_cores);
        printf("==========================================================================================\n");

        /* Core topology */
        for (uint32_t c = 0; c < snapshot.num_cores && c < 4; c++) {
            printf("Core %u: [%4.1f%%] (CtxSw: %6lu/s)%s",
                   c, snapshot.core_utilization[c],
                   (unsigned long)snapshot.core_ctxsw_per_sec[c],
                   (c % 2 == 1 || c == snapshot.num_cores - 1) ? "\n" : "   │   ");
        }

        float ram_pct = (snapshot.ram_total_mb > 0)
                        ? ((float)snapshot.ram_used_mb / (float)snapshot.ram_total_mb) * 100.0f
                        : 0.0f;

        printf("RAM: %lu MB / %lu MB (%4.1f%%) │ ResMgr Pool: %lu KB │ Total Msg Rate: %lu msgs/sec\n",
               (unsigned long)snapshot.ram_used_mb,
               (unsigned long)snapshot.ram_total_mb,
               ram_pct,
               (unsigned long)snapshot.resmgr_pool_kb,
               (unsigned long)snapshot.total_channel_msgs_sec);
        printf("------------------------------------------------------------------------------------------\n");

        /* 2. Process & Thread State Matrix */
        printf("PID    TID  PRI   STATE             CPU%%    CHID   PULSES/S   COMMAND\n");
        printf("------------------------------------------------------------------------------------------\n");

        for (int i = 0; i < snapshot.num_processes; i++) {
            qnx_process_info_t *p = &snapshot.processes[i];
            const char *state_str = qnx_thread_state_to_string(p->threads[0].state);

            if (p->threads[0].state == QNX_THREAD_REPLY_BLOCKED) {
                printf("%-6d %-4d %-5d %-17s %5.1f%%  0x%02x   %-10lu %-20s (Stall: %.1fms)\n",
                       p->pid, p->threads[0].tid, p->threads[0].priority,
                       state_str, p->cpu_percent, p->active_chid,
                       (unsigned long)p->total_pulses_received, p->name,
                       p->threads[0].reply_stall_ms);
            } else {
                printf("%-6d %-4d %-5d %-17s %5.1f%%  0x%02x   %-10lu %-20s\n",
                       p->pid, p->threads[0].tid, p->threads[0].priority,
                       state_str, p->cpu_percent, p->active_chid,
                       (unsigned long)p->total_pulses_received, p->name);
            }
        }

        /* 3. Channel & Connection Matrix */
        printf("\n[Channel & Connection Matrix (Synchronous MsgSend/MsgReceive)]\n");
        printf("%-8s %-12s %-18s %-16s %-10s %-22s\n",
               "CHID", "OWNER PID", "RECEIVE THREAD", "BOUND CLIENTS", "MSG/SEC", "STATUS");
        printf("------------------------------------------------------------------------------------------\n");

        for (int i = 0; i < snapshot.num_channels; i++) {
            qnx_channel_info_t *c = &snapshot.channels[i];
            if (c->is_stalled) {
                char stall_buf[32];
                snprintf(stall_buf, sizeof(stall_buf), "STALL (%.1fms)", c->max_stall_latency_ms);
                printf("0x%02x     %-12d TID %-2d (PRI %-3d)   %-16d %-10lu %-22s\n",
                       c->chid, c->owner_pid, c->receive_tid, c->receive_priority,
                       c->bound_connections, (unsigned long)c->messages_per_sec, stall_buf);
            } else {
                printf("0x%02x     %-12d TID %-2d (PRI %-3d)   %-16d %-10lu %-22s\n",
                       c->chid, c->owner_pid, c->receive_tid, c->receive_priority,
                       c->bound_connections, (unsigned long)c->messages_per_sec, "DETERMINISTIC");
            }
        }

        /* 4. Adaptive Partitioning System */
        printf("\n[Adaptive Partitioning System (APS) CPU Budgets]\n");
        for (int i = 0; i < snapshot.num_partitions; i++) {
            qnx_aps_partition_t *pt = &snapshot.partitions[i];
            printf("• %-22s Budget: %4.1f%% | Current: %4.1f%% | Max Jitter: <%lu µs %s\n",
                   pt->name, pt->budget_percent, pt->current_usage_percent,
                   (unsigned long)pt->max_jitter_us, pt->is_critical ? "[HARD REAL-TIME]" : "");
        }

        /* 5. Resource Manager Tree */
        printf("\n[Resource Manager Tree (/dev Hierarchy & POSIX Handlers)]\n");
        for (int i = 0; i < snapshot.num_dev_nodes; i++) {
            qnx_resmgr_node_t *n = &snapshot.dev_nodes[i];
            printf("• %-24s Owner: %-18s Handlers: %-25s (Req/s: %lu)\n",
                   n->path, n->owner_name, n->handlers, (unsigned long)n->io_requests_per_sec);
        }

        iteration++;
        if (max_iterations > 0 && iteration >= max_iterations) {
            break;
        }

        if (batch_mode) {
            break;
        }

        /* Sleep between refresh cycles */
        sleep_ms((int)(delay_sec * 1000.0f));
    }

    printf("\n[qnx-top] Stopping diagnostic monitor...\n");
    qnx_top_stop_resmgr();
    qnx_top_cleanup();
    return 0;
}
