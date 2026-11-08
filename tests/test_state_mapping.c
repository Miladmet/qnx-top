/**
 * @file test_state_mapping.c
 * @brief Unit tests for state mappings, snapshot invariants, stall tracking and thread safety
 *
 * TEST_ASSERT is deliberately NOT based on <assert.h>: assert() compiles to
 * nothing when NDEBUG is defined (Release builds), which would make the tests
 * pass without checking anything.
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__QNX__)
    #include <pthread.h>
    #define HAVE_PTHREADS 1
#endif

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (%s:%d): %s\n", #cond, __FILE__, __LINE__, msg); \
        exit(1); \
    } \
} while(0)

#define MS_NS 1000000ULL

static void test_state_strings(void) {
    printf("[TEST] Testing QNX thread state conversions...\n");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_RUNNING), "RUNNING") == 0, "RUNNING");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_READY), "READY") == 0, "READY");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_RECEIVE_BLOCKED), "RECEIVE") == 0, "RECEIVE");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_REPLY_BLOCKED), "REPLY-blocked") == 0, "REPLY");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_SEND_BLOCKED), "SEND-blocked") == 0, "SEND");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_NANOSLEEP), "NANOSLEEP") == 0, "NANOSLEEP");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_MUTEX), "MUTEX") == 0, "MUTEX");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_CONDVAR), "CONDVAR") == 0, "CONDVAR");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_SEM), "SEM") == 0, "SEM");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_INTR), "INTR") == 0, "INTR");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_STOPPED), "STOPPED") == 0, "STOPPED");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_DEAD), "DEAD") == 0, "DEAD");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_JOIN), "JOIN") == 0, "JOIN");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_WAITPAGE), "WAITPAGE") == 0, "WAITPAGE");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string(QNX_THREAD_UNKNOWN), "UNKNOWN") == 0, "UNKNOWN enum");
    TEST_ASSERT(strcmp(qnx_thread_state_to_string((qnx_thread_state_t)999), "UNKNOWN") == 0, "out of range");
    printf("  -> PASS\n");
}

static void test_snapshot_collection(void) {
    printf("[TEST] Testing snapshot collection and invariants...\n");
    TEST_ASSERT(qnx_top_init() == 0, "init");

    qnx_system_snapshot_t *snapshot = malloc(sizeof(*snapshot));
    TEST_ASSERT(snapshot != NULL, "allocate snapshot");
    memset(snapshot, 0xAB, sizeof(*snapshot)); /* collector must fully reinitialise it */

    TEST_ASSERT(qnx_top_collect_snapshot(snapshot) == 0, "collect");
    TEST_ASSERT(qnx_top_collect_snapshot(NULL) == -1, "NULL snapshot rejected");
    TEST_ASSERT(snapshot->num_cores > 0 && snapshot->num_cores <= MAX_CORES, "core count");
    TEST_ASSERT(snapshot->num_processes > 0 && snapshot->num_processes <= MAX_PROCESSES, "process count");
    TEST_ASSERT(snapshot->num_channels >= 0 && snapshot->num_channels <= MAX_CHANNELS, "channel count");
    TEST_ASSERT(snapshot->num_partitions >= 0 && snapshot->num_partitions <= MAX_PARTITIONS, "partition count");
    TEST_ASSERT(snapshot->num_dev_nodes >= 0 && snapshot->num_dev_nodes <= MAX_DEV_NODES, "dev count");

    if (!snapshot->is_native_qnx) {
        TEST_ASSERT(snapshot->avail.ram_total && snapshot->ram_total_mb > 0, "simulated RAM total");
    }

    for (int i = 0; i < snapshot->num_processes; i++) {
        const qnx_process_info_t *p = &snapshot->processes[i];
        TEST_ASSERT(strlen(p->name) > 0, "process name not empty");
        TEST_ASSERT(p->num_threads > 0 && p->num_threads <= MAX_THREADS_PER_PROC, "thread count");
    }

    /* Channels are owned by the SERVER and carry the clients waiting on them. */
    for (int i = 0; i < snapshot->num_channels; i++) {
        const qnx_channel_info_t *c = &snapshot->channels[i];
        TEST_ASSERT(c->chid > 0, "channel id");
        TEST_ASSERT(c->owner_pid > 0, "channel has a server owner");
        TEST_ASSERT(c->blocked_clients >= 0, "blocked clients non-negative");
        TEST_ASSERT(c->is_stalled == (c->max_stall_latency_ms > QNX_TOP_STALL_THRESHOLD_MS),
                    "stall flag matches threshold");
    }

    /* Every REPLY-blocked thread must point at a channel in the matrix, owned by its server. */
    for (int i = 0; i < snapshot->num_processes; i++) {
        const qnx_process_info_t *p = &snapshot->processes[i];
        for (int t = 0; t < p->num_threads; t++) {
            const qnx_thread_info_t *th = &p->threads[t];
            if (th->state != QNX_THREAD_REPLY_BLOCKED) continue;
            TEST_ASSERT(th->blocked_on_pid > 0 && th->blocked_on_pid != p->pid,
                        "reply-blocked client names a different server process");
            bool found = false;
            for (int c = 0; c < snapshot->num_channels; c++) {
                if (snapshot->channels[c].owner_pid == th->blocked_on_pid &&
                    snapshot->channels[c].chid == th->blocked_on_chid) {
                    found = true;
                }
            }
            TEST_ASSERT(found, "client's server channel is present in channel matrix");
        }
    }

    free(snapshot);
    printf("  -> PASS\n");
}

static void test_stall_tracker(void) {
    printf("[TEST] Testing REPLY stall tracking...\n");
    qnx_stall_tracker_reset();
    TEST_ASSERT(qnx_stall_tracker_size() == 0, "empty after reset");

    const uint64_t t0 = 5000 * MS_NS;

    /* First sighting in REPLY records the start: no stall is invented. */
    float s = qnx_stall_tracker_observe(10, 1, true, t0);
    TEST_ASSERT(s == 0.0f, "first observation has zero stall");

    /* Still blocked 7.5 ms later. */
    s = qnx_stall_tracker_observe(10, 1, true, t0 + 7 * MS_NS + MS_NS / 2);
    TEST_ASSERT(s > 7.49f && s < 7.51f, "stall grows from first sighting");

    /* Another thread is tracked independently. */
    s = qnx_stall_tracker_observe(10, 2, true, t0 + 8 * MS_NS);
    TEST_ASSERT(s == 0.0f, "second thread starts its own stall");

    /* Leaving REPLY resets the start. */
    s = qnx_stall_tracker_observe(10, 1, false, t0 + 10 * MS_NS);
    TEST_ASSERT(s == 0.0f, "not in REPLY => no stall");

    /* Re-entering REPLY starts a fresh stall, not a continuation. */
    s = qnx_stall_tracker_observe(10, 1, true, t0 + 20 * MS_NS);
    TEST_ASSERT(s == 0.0f, "re-entry starts from zero");
    s = qnx_stall_tracker_observe(10, 1, true, t0 + 23 * MS_NS);
    TEST_ASSERT(s > 2.99f && s < 3.01f, "fresh stall measured from re-entry");

    /* Sweep drops threads that were not observed since the previous sweep. */
    TEST_ASSERT(qnx_stall_tracker_size() == 2, "two tracked threads");
    qnx_stall_tracker_sweep();               /* both were seen: kept, flags cleared */
    TEST_ASSERT(qnx_stall_tracker_size() == 2, "kept after first sweep");
    qnx_stall_tracker_observe(10, 1, true, t0 + 30 * MS_NS);
    qnx_stall_tracker_sweep();               /* tid 2 not seen: dropped */
    TEST_ASSERT(qnx_stall_tracker_size() == 1, "unseen thread evicted");

    qnx_stall_tracker_reset();
    printf("  -> PASS\n");
}

#ifdef HAVE_PTHREADS
#define WORKERS 4
#define ITERATIONS 2000

static void *tracker_worker(void *arg) {
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        uint64_t now = (uint64_t)(i + 1) * MS_NS;
        qnx_stall_tracker_observe(100 + id, i % 8, (i % 3) != 0, now);
        if (i % 50 == 0) qnx_stall_tracker_sweep();
    }
    return NULL;
}

static void *collect_worker(void *arg) {
    (void)arg;
    qnx_system_snapshot_t *snapshot = malloc(sizeof(*snapshot));
    if (!snapshot) return (void *)1;
    int bad = 0;
    for (int i = 0; i < 100; i++) {
        if (qnx_top_collect_snapshot(snapshot) != 0) bad++;
    }
    free(snapshot);
    return (void *)(intptr_t)bad;
}

static void test_concurrent_access(void) {
    printf("[TEST] Testing concurrent collector/tracker access...\n");
    qnx_stall_tracker_reset();

    pthread_t tracker[WORKERS];
    pthread_t collector[2];
    for (int i = 0; i < WORKERS; i++) {
        TEST_ASSERT(pthread_create(&tracker[i], NULL, tracker_worker, (void *)(intptr_t)i) == 0,
                    "spawn tracker thread");
    }
    for (int i = 0; i < 2; i++) {
        TEST_ASSERT(pthread_create(&collector[i], NULL, collect_worker, NULL) == 0,
                    "spawn collector thread");
    }
    for (int i = 0; i < WORKERS; i++) pthread_join(tracker[i], NULL);
    for (int i = 0; i < 2; i++) {
        void *ret = NULL;
        pthread_join(collector[i], &ret);
        TEST_ASSERT(ret == NULL, "concurrent collect succeeded");
    }

    TEST_ASSERT(qnx_stall_tracker_size() >= 0 && qnx_stall_tracker_size() <= MAX_PROCESSES * 8,
                "tracker size stays bounded");
    qnx_stall_tracker_reset();
    printf("  -> PASS\n");
}
#endif

int main(void) {
    printf("===================================================================\n");
    printf(" Running qnx-top Verification Test Suite\n");
    printf("===================================================================\n");

    test_state_strings();
    test_snapshot_collection();
    test_stall_tracker();
#ifdef HAVE_PTHREADS
    test_concurrent_access();
#endif

    printf("\n===================================================================\n");
    printf(" ALL TESTS PASSED\n");
    printf("===================================================================\n");
    return 0;
}
