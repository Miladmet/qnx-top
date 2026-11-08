/**
 * @file main.c
 * @brief Entry point for qnx-top native QNX 8.0 binary
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    printf("===================================================================\n");
    printf("  qnx-top v%s: Real-Time Microkernel Observability for QNX 8.0\n", QNX_TOP_VERSION);
    printf("  Target: BlackBerry QNX Neutrino 8.0 (Everywhere / QEMU)\n");
    printf("  License: MIT License\n");
    printf("===================================================================\n\n");

    if (qnx_top_init() != 0) {
        fprintf(stderr, "Failed to initialize qnx-top.\n");
        return 1;
    }

    /* Start the native resource manager */
    qnx_top_start_resmgr();

    qnx_system_snapshot_t snapshot;
    if (qnx_top_collect_snapshot(&snapshot) != 0) {
        fprintf(stderr, "Failed to collect initial system snapshot.\n");
        return 1;
    }

    printf("[System Topology]\n");
    printf("  SMP Cores: %u  |  RAM: %lu MB / %lu MB  |  ResMgr Pool: %lu KB\n\n",
           snapshot.num_cores, (unsigned long)snapshot.ram_used_mb,
           (unsigned long)snapshot.ram_total_mb, (unsigned long)snapshot.resmgr_pool_kb);

    printf("[Active QNX Thread & Process Matrix]\n");
    printf("  %-6s %-4s %-6s %-16s %-8s %-10s %-25s\n",
           "PID", "TID", "PRI", "STATE", "CPU%", "CHID", "COMMAND");
    printf("  -----------------------------------------------------------------\n");

    for (int i = 0; i < snapshot.num_processes; i++) {
        qnx_process_info_t *p = &snapshot.processes[i];
        const char *state_str = qnx_thread_state_to_string(p->threads[0].state);
        printf("  %-6d %-4d %-6d %-16s %-8.1f 0x%02x       %-25s\n",
               p->pid, p->threads[0].tid, p->threads[0].priority,
               state_str, p->cpu_percent, p->active_chid, p->name);
    }

    printf("\n[QNX Native Channel & Connection Matrix]\n");
    printf("  %-6s %-12s %-18s %-16s %-10s %-20s\n",
           "CHID", "OWNER PID", "RECEIVE THREAD", "BOUND CLIENTS", "MSG/SEC", "STATUS");
    printf("  --------------------------------------------------------------------------------\n");

    for (int i = 0; i < snapshot.num_channels; i++) {
        qnx_channel_info_t *c = &snapshot.channels[i];
        printf("  0x%02x   %-12d TID %d (PRI %d)     %-16d %-10lu %s\n",
               c->chid, c->owner_pid, c->receive_tid, c->receive_priority,
               c->bound_connections, (unsigned long)c->messages_per_sec,
               c->is_stalled ? "STALL DETECTED" : "DETERMINISTIC");
    }

    printf("\n[Adaptive Partitioning System (APS)]\n");
    for (int i = 0; i < snapshot.num_partitions; i++) {
        qnx_aps_partition_t *pt = &snapshot.partitions[i];
        printf("  • %-20s Budget: %4.1f%% | Usage: %4.1f%% | Max Jitter: <%lu µs %s\n",
               pt->name, pt->budget_percent, pt->current_usage_percent,
               (unsigned long)pt->max_jitter_us, pt->is_critical ? "[CRITICAL]" : "");
    }

    printf("\n[Registered Resource Managers under /dev]\n");
    for (int i = 0; i < snapshot.num_dev_nodes; i++) {
        qnx_resmgr_node_t *n = &snapshot.dev_nodes[i];
        printf("  • %-22s -> Owner: %-18s (Req/s: %lu)\n",
               n->path, n->owner_name, (unsigned long)n->io_requests_per_sec);
    }

    printf("\n[OK] Snapshot cycle complete. Ready for interactive monitoring loop.\n");
    qnx_top_cleanup();
    return 0;
}
