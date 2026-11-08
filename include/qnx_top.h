/**
 * @file qnx_top.h
 * @brief Header definitions for qnx-top: Real-Time Microkernel Telemetry for QNX 8.0
 * 
 * Target Platform: BlackBerry QNX Neutrino 8.0 (QNX Everywhere)
 * License: MIT License
 */

#ifndef QNX_TOP_H
#define QNX_TOP_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __QNX__
    #include <sys/neutrino.h>
    #include <sys/procfs.h>
    #include <sys/resmgr.h>
    #include <sys/iofunc.h>
    #include <sys/procmgr.h>
#endif

#define QNX_TOP_VERSION "1.0.0"
#define MAX_PROCESSES 256
#define MAX_THREADS_PER_PROC 32
#define MAX_CHANNELS 128
#define MAX_DEV_NODES 64
#define MAX_NAME_LEN 64

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
    QNX_THREAD_NET_REPLY
} qnx_thread_state_t;

/**
 * Thread telemetry structure
 */
typedef struct {
    int tid;
    int priority;
    int policy;              /* SCHED_FIFO, SCHED_RR, SCHED_OTHER */
    qnx_thread_state_t state;
    uint64_t cpu_time_ns;
    uint32_t blocked_on_chid;
    uint64_t reply_stall_ns; /* Microsecond latency if in REPLY-blocked state */
} qnx_thread_info_t;

/**
 * Process telemetry structure
 */
typedef struct {
    int pid;
    int ppid;
    char name[MAX_NAME_LEN];
    uint64_t memory_rss_bytes;
    uint64_t memory_vsize_bytes;
    float cpu_percent;
    int num_threads;
    qnx_thread_info_t threads[MAX_THREADS_PER_PROC];
    int active_chid;
    uint64_t total_pulses_received;
} qnx_process_info_t;

/**
 * QNX Native IPC Channel structure
 */
typedef struct {
    int chid;
    int owner_pid;
    char owner_name[MAX_NAME_LEN];
    int receive_tid;
    int receive_priority;
    int bound_connections;
    uint64_t messages_per_sec;
    bool is_stalled;
    float max_stall_latency_ms;
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
    int owner_pid;
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
    uint32_t num_cores;
    float core_utilization[16];
    uint64_t core_ctxsw_per_sec[16];
    uint64_t ram_total_mb;
    uint64_t ram_used_mb;
    uint64_t resmgr_pool_kb;
    uint64_t shm_pool_kb;
    
    int num_processes;
    qnx_process_info_t processes[MAX_PROCESSES];
    
    int num_channels;
    qnx_channel_info_t channels[MAX_CHANNELS];
    
    int num_partitions;
    qnx_aps_partition_t partitions[8];
    
    int num_dev_nodes;
    qnx_resmgr_node_t dev_nodes[MAX_DEV_NODES];
} qnx_system_snapshot_t;

/* Public API Prototypes */
int qnx_top_init(void);
int qnx_top_collect_snapshot(qnx_system_snapshot_t *snapshot);
const char* qnx_thread_state_to_string(qnx_thread_state_t state);
int qnx_top_start_resmgr(void);
void qnx_top_cleanup(void);

#endif /* QNX_TOP_H */
