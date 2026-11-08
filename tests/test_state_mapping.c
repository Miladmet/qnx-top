/**
 * @file test_state_mapping.c
 * @brief Unit tests for thread state mappings, telemetry collections, and stall logic
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_state_strings(void) {
    printf("[TEST] Testing QNX thread state conversions...\n");
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_RUNNING), "RUNNING") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_READY), "READY") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_RECEIVE_BLOCKED), "RECEIVE") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_REPLY_BLOCKED), "REPLY-blocked") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_SEND_BLOCKED), "SEND-blocked") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_NANOSLEEP), "NANOSLEEP") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_MUTEX), "MUTEX") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_CONDVAR), "CONDVAR") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_SEM), "SEM") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_INTR), "INTR") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_STOPPED), "STOPPED") == 0);
    assert(strcmp(qnx_thread_state_to_string(QNX_THREAD_DEAD), "DEAD") == 0);
    assert(strcmp(qnx_thread_state_to_string((qnx_thread_state_t)999), "UNKNOWN") == 0);
    printf("  -> PASS: All 13 thread states converted accurately.\n");
}

static void test_snapshot_collection(void) {
    printf("[TEST] Testing snapshot initialization and collection...\n");
    assert(qnx_top_init() == 0);

    qnx_system_snapshot_t snapshot;
    memset(&snapshot, 0, sizeof(snapshot));

    int rc = qnx_top_collect_snapshot(&snapshot);
    assert(rc == 0);
    assert(snapshot.num_cores > 0);
    assert(snapshot.num_processes > 0);
    assert(snapshot.ram_total_mb > 0);
    assert(snapshot.num_partitions > 0);
    assert(snapshot.num_dev_nodes > 0);

    /* Verify that process names and thread states are not empty */
    for (int i = 0; i < snapshot.num_processes; i++) {
        assert(strlen(snapshot.processes[i].name) > 0);
        assert(snapshot.processes[i].num_threads > 0);
    }

    /* Verify channel matrix contains bound clients and stall flags */
    for (int i = 0; i < snapshot.num_channels; i++) {
        assert(snapshot.channels[i].chid > 0);
        if (snapshot.channels[i].is_stalled) {
            assert(snapshot.channels[i].max_stall_latency_ms > 0.0f);
        }
    }

    printf("  -> PASS: Snapshot collection and data validation succeeded.\n");
}

int main(void) {
    printf("===================================================================\n");
    printf(" Running qnx-top Verification Test Suite\n");
    printf("===================================================================\n");

    test_state_strings();
    test_snapshot_collection();

    printf("\n===================================================================\n");
    printf(" ALL TESTS PASSED SUCCESSFULLY (100%% Green)\n");
    printf("===================================================================\n");
    return 0;
}
