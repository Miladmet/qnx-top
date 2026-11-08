/**
 * @file qnx_top.h
 * @brief Header definitions for qnx-top: Real-Time Microkernel Telemetry for QNX 8.0
 *
 * Target Platform: BlackBerry QNX Neutrino 8.0 (QNX Everywhere)
 * License: MIT License
 *
 * Metric honesty rule: every metric that cannot be measured on the current
 * platform is flagged unavailable in qnx_metric_availability_t. Consumers must
 * print "N/A" for those metrics and never display the stored value.
 */

#ifndef QNX_TOP_H
#define QNX_TOP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define QNX_TOP_VERSION "1.0.0"
#define MAX_PROCESSES 256
#define MAX_THREADS_PER_PROC 32
#define MAX_CHANNELS 128
#define MAX_DEV_NODES 64
#define MAX_NAME_LEN 64
#define MAX_CORES 16
#define MAX_PARTITIONS 8

/** A REPLY-blocked client is reported as stalled once it waits longer than this. */
#define QNX_TOP_STALL_THRESHOLD_MS 2.0f

/**
 * Standard QNX Thread State Identifiers
 * Corresponds to QNX Neutrino kernel scheduler states
 */
typedef enum {
    QNX_THREAD_DEAD = 0,
    QNX_THREAD_RUNNING,
    QNX_THREAD_READY,
    QNX_THREAD_STOPPED,
    QNX_THREAD_SEND_BLOCKED,
    QNX_THREAD_RECEIVE_BLOCKED,
    QNX_THREAD_REPLY_BLOCKED,
    QNX_THREAD_WAITPAGE,
    QNX_THREAD_STACK,
    QNX_THREAD_NANOSLEEP,
    QNX_THREAD_MUTEX,
    QNX_THREAD_CONDVAR,
    QNX_THREAD_JOIN,
    QNX_THREAD_INTR,
    QNX_THREAD_SEM,
    QNX_THREAD_WAITCTX,
    QNX_THREAD_NET_SEND,
    QNX_THREAD_NET_REPLY,
    QNX_THREAD_UNKNOWN          /* kernel state not mapped by qnx-top */
} qnx_thread_state_t;

/**
 * Which metrics in a snapshot are real measurements.
 * false => the value is not measured and must be shown as "N/A".
 */
typedef struct {
    bool core_util;         /* core_utilization[] */
    bool core_ctxsw;        /* core_ctxsw_per_sec[] (needs TraceEvent) */
    bool ram_total;         /* ram_total_mb */
    bool ram_used;          /* ram_used_mb */
    bool pools;             /* resmgr_pool_kb, shm_pool_kb */
    bool msg_rates;         /* channel messages_per_sec, total_channel_msgs_sec */
    bool pulses;            /* process total_pulses_received */
    bool dev_details;       /* /dev node owner, handlers, request rate */
} qnx_metric_availability_t;

/**
 * Thread telemetry structure
 */
typedef struct {
    int tid;
    int priority;
    int policy;                 /* SCHED_FIFO, SCHED_RR, SCHED_OTHER */
    qnx_thread_state_t state;
    uint64_t cpu_time_ns;       /* Cumulative user+system time in ns */
    int blocked_on_pid;         /* Server PID when REPLY-blocked, else 0 */
    int blocked_on_chid;        /* Server channel ID when REPLY-blocked, else 0 */
    float reply_stall_ms;       /* Time continuously REPLY-blocked, in ms */
} qnx_thread_info_t;

/**
 * Process telemetry structure
 */
typedef struct {
    int pid;
    int ppid;
    char name[MAX_NAME_LEN];
    uint64_t memory_vsize_bytes;/* Address space size; resident set is not collected */
    float cpu_percent;          /* Delta CPU utilization (valid only if cpu_valid) */
    bool cpu_valid;             /* false on the first sample of a process */
    int num_threads;            /* Number of entries stored in threads[] */
    qnx_thread_info_t threads[MAX_THREADS_PER_PROC];
    int active_chid;            /* Channel this process receives on, 0 if none seen */
    uint64_t total_pulses_received; /* Valid only if availability.pulses */
} qnx_process_info_t;

/**
 * QNX IPC channel with clients blocked on it.
 * The owner is the SERVER process hosting the channel. Clients are the
 * REPLY-blocked threads waiting on it.
 */
typedef struct {
    int chid;
    int owner_pid;
    char owner_name[MAX_NAME_LEN];
    int receive_tid;            /* Server thread in RECEIVE on chid, -1 if none seen */
    int receive_priority;       /* -1 if receive_tid is unknown */
    int blocked_clients;        /* Client threads currently REPLY-blocked on it */
    uint64_t messages_per_sec;  /* Valid only if availability.msg_rates */
    bool is_stalled;
    float max_stall_latency_ms; /* Longest current client stall */
} qnx_channel_info_t;

/**
 * Adaptive Partitioning System (APS) telemetry
 */
typedef struct {
    char name[MAX_NAME_LEN];
    float budget_percent;
    float current_usage_percent;
    float peak_usage_percent;
    uint64_t max_jitter_us;
    bool is_critical;
} qnx_aps_partition_t;

/**
 * Resource Manager /dev node structure
 */
typedef struct {
    char path[MAX_NAME_LEN];
    int owner_pid;              /* -1 if unknown */
    char owner_name[MAX_NAME_LEN];
    int major;
    int minor;
    char handlers[MAX_NAME_LEN];
    uint64_t io_requests_per_sec;
} qnx_resmgr_node_t;

/**
 * Complete System Snapshot
 */
typedef struct {
    bool is_native_qnx;         /* True if running on genuine QNX kernel */
    qnx_metric_availability_t avail;
    uint64_t timestamp_ns;
    uint32_t num_cores;
    float core_utilization[MAX_CORES];
    uint64_t core_ctxsw_per_sec[MAX_CORES];
    uint64_t ram_total_mb;
    uint64_t ram_used_mb;
    uint64_t resmgr_pool_kb;
    uint64_t shm_pool_kb;
    uint64_t total_channel_msgs_sec;

    int num_processes;
    qnx_process_info_t processes[MAX_PROCESSES];

    int num_channels;
    qnx_channel_info_t channels[MAX_CHANNELS];

    bool aps_active;            /* APS scheduler detected on the running kernel */
    int num_partitions;         /* 0 unless per-partition data was collected */
    qnx_aps_partition_t partitions[MAX_PARTITIONS];

    int num_dev_nodes;
    qnx_resmgr_node_t dev_nodes[MAX_DEV_NODES];
} qnx_system_snapshot_t;

/* Public API Prototypes */
int qnx_top_init(void);
/** Thread-safe: may be called concurrently from the main loop and the resmgr thread. */
int qnx_top_collect_snapshot(qnx_system_snapshot_t *snapshot);
const char* qnx_thread_state_to_string(qnx_thread_state_t state);
int qnx_top_start_resmgr(void);
void qnx_top_stop_resmgr(void);
void qnx_top_cleanup(void);

/*
 * Stall tracker (platform independent, thread-safe, unit-tested).
 * A thread is first seen REPLY-blocked at time T: the start is recorded as T.
 * While it stays REPLY-blocked the stall is (now - T). Leaving the state resets it.
 */
void qnx_stall_tracker_reset(void);
/** Records one observation and returns the current stall in ms (0 when not REPLY-blocked). */
float qnx_stall_tracker_observe(int pid, int tid, bool in_reply_state, uint64_t now_ns);
/** Drops tracker entries not observed since the previous sweep. */
void qnx_stall_tracker_sweep(void);
/** Number of threads currently tracked. */
int qnx_stall_tracker_size(void);

#endif /* QNX_TOP_H */
