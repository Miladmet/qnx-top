/**
 * @file qnx_collector.c
 * @brief Telemetry collector using native QNX 8.0 Neutrino procfs APIs
 *
 * Native mode reports only what the kernel interfaces below actually provide:
 *  - /proc/<pid>/as  DCMD_PROC_INFO, DCMD_PROC_MAPDEBUG_BASE, DCMD_PROC_TIDSTATUS
 *  - sysconf() physical memory page counts (when supported)
 *  - SchedCtl() APS probe
 * Everything else is flagged unavailable in snapshot->avail and shown as "N/A".
 *
 * Thread safety: all history state (g_history, g_history_count) is guarded by
 * g_history_mutex. qnx_top_collect_snapshot() holds the lock for the whole
 * collection and has a single exit path so the lock is always released.
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <fcntl.h>
    #include <dirent.h>
    #include <sys/stat.h>
#endif

#if defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
    #include <pthread.h>
#endif

#ifdef __QNX__
    #include <stddef.h>
    #include <limits.h>
    #include <errno.h>
    #include <devctl.h>
    #include <sched.h>
    #include <sys/neutrino.h>
    #include <sys/procfs.h>
    #include <sys/states.h>
    #include <sys/syspage.h>
    #if defined(__has_include)
        #if __has_include(<sys/sched_aps.h>)
            #include <sys/sched_aps.h>
        #endif
    #endif
#endif

/* ------------------------------------------------------------------------- */
/* History (CPU delta + REPLY stall tracking), guarded by g_history_mutex     */
/* ------------------------------------------------------------------------- */

#define MAX_HISTORY (MAX_PROCESSES * 8)

typedef struct {
    int pid;
    int tid;
    uint64_t last_sutime_ns;
    uint64_t last_sample_time_ns;
    uint64_t reply_block_start_ns;  /* 0 => not currently REPLY-blocked */
    bool seen;                      /* observed since the last sweep */
} proc_cpu_history_t;

typedef struct {
    float stall_ms;
    bool had_prev;                  /* a previous sample existed for this thread */
    uint64_t delta_sutime_ns;
    uint64_t delta_wall_ns;
} history_obs_t;

static proc_cpu_history_t g_history[MAX_HISTORY];
static int g_history_count = 0;

#if defined(__QNX__) || defined(__unix__) || defined(__APPLE__)
static pthread_mutex_t g_history_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Disable cancellation while holding the lock so a pthread_cancel() of the
 * resmgr thread can never leave the mutex locked. */
static void history_lock(int *old_cancel_state) {
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, old_cancel_state);
    pthread_mutex_lock(&g_history_mutex);
}

static void history_unlock(int old_cancel_state) {
    int ignored;
    pthread_mutex_unlock(&g_history_mutex);
    pthread_setcancelstate(old_cancel_state, &ignored);
}
#else
static void history_lock(int *old_cancel_state) { *old_cancel_state = 0; }
static void history_unlock(int old_cancel_state) { (void)old_cancel_state; }
#endif

/* The functions below require g_history_mutex to be held by the caller. */

static void history_clear_locked(void) {
    memset(g_history, 0, sizeof(g_history));
    g_history_count = 0;
}

static proc_cpu_history_t *history_get_locked(int pid, int tid, bool *is_new) {
    for (int i = 0; i < g_history_count; i++) {
        if (g_history[i].pid == pid && g_history[i].tid == tid) {
            *is_new = false;
            return &g_history[i];
        }
    }
    if (g_history_count >= MAX_HISTORY) {
        return NULL;
    }
    proc_cpu_history_t *h = &g_history[g_history_count++];
    memset(h, 0, sizeof(*h));
    h->pid = pid;
    h->tid = tid;
    *is_new = true;
    return h;
}

static void history_observe_locked(int pid, int tid, uint64_t sutime_ns, bool in_reply,
                                   uint64_t now_ns, history_obs_t *out) {
    memset(out, 0, sizeof(*out));

    bool is_new = false;
    proc_cpu_history_t *h = history_get_locked(pid, tid, &is_new);
    if (!h) {
        return; /* table full: no history, stall/CPU unknown for this thread */
    }

    if (!is_new) {
        out->had_prev = true;
        if (now_ns > h->last_sample_time_ns) {
            out->delta_wall_ns = now_ns - h->last_sample_time_ns;
        }
        if (sutime_ns >= h->last_sutime_ns) {
            out->delta_sutime_ns = sutime_ns - h->last_sutime_ns;
        }
    }

    if (in_reply) {
        if (h->reply_block_start_ns == 0) {
            h->reply_block_start_ns = now_ns;   /* first observation in STATE_REPLY */
        }
        if (now_ns > h->reply_block_start_ns) {
            out->stall_ms = (float)(now_ns - h->reply_block_start_ns) / 1000000.0f;
        }
    } else {
        h->reply_block_start_ns = 0;            /* left STATE_REPLY: reset */
    }

    h->last_sutime_ns = sutime_ns;
    h->last_sample_time_ns = now_ns;
    h->seen = true;
}

static void history_sweep_locked(void) {
    int kept = 0;
    for (int i = 0; i < g_history_count; i++) {
        if (g_history[i].seen) {
            g_history[i].seen = false;
            if (kept != i) {
                g_history[kept] = g_history[i];
            }
            kept++;
        }
    }
    g_history_count = kept;
}

void qnx_stall_tracker_reset(void) {
    int cs;
    history_lock(&cs);
    history_clear_locked();
    history_unlock(cs);
}

float qnx_stall_tracker_observe(int pid, int tid, bool in_reply_state, uint64_t now_ns) {
    history_obs_t obs;
    int cs;
    history_lock(&cs);
    history_observe_locked(pid, tid, 0, in_reply_state, now_ns, &obs);
    history_unlock(cs);
    return obs.stall_ms;
}

void qnx_stall_tracker_sweep(void) {
    int cs;
    history_lock(&cs);
    history_sweep_locked();
    history_unlock(cs);
}

int qnx_stall_tracker_size(void) {
    int cs;
    history_lock(&cs);
    int n = g_history_count;
    history_unlock(cs);
    return n;
}

/* ------------------------------------------------------------------------- */
/* Helpers                                                                   */
/* ------------------------------------------------------------------------- */

static uint64_t get_time_now_ns(void) {
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (uint64_t)((count.QuadPart * 1000000000ULL) / freq.QuadPart);
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ((uint64_t)ts.tv_sec * 1000000000ULL) + (uint64_t)ts.tv_nsec;
#endif
}

/* Bounded, always NUL-terminated copy. */
static void copy_str(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    size_t n = 0;
    while (src[n] != '\0' && n < dst_size - 1) n++;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

const char* qnx_thread_state_to_string(qnx_thread_state_t state) {
    switch (state) {
        case QNX_THREAD_RUNNING:         return "RUNNING";
        case QNX_THREAD_READY:           return "READY";
        case QNX_THREAD_RECEIVE_BLOCKED: return "RECEIVE";
        case QNX_THREAD_REPLY_BLOCKED:   return "REPLY-blocked";
        case QNX_THREAD_SEND_BLOCKED:    return "SEND-blocked";
        case QNX_THREAD_NANOSLEEP:       return "NANOSLEEP";
        case QNX_THREAD_MUTEX:           return "MUTEX";
        case QNX_THREAD_CONDVAR:         return "CONDVAR";
        case QNX_THREAD_SEM:             return "SEM";
        case QNX_THREAD_INTR:            return "INTR";
        case QNX_THREAD_STOPPED:         return "STOPPED";
        case QNX_THREAD_DEAD:            return "DEAD";
        case QNX_THREAD_JOIN:            return "JOIN";
        case QNX_THREAD_WAITPAGE:        return "WAITPAGE";
        case QNX_THREAD_STACK:           return "STACK";
        case QNX_THREAD_WAITCTX:         return "WAITCTX";
        case QNX_THREAD_NET_SEND:        return "NET-SEND";
        case QNX_THREAD_NET_REPLY:       return "NET-REPLY";
        case QNX_THREAD_UNKNOWN:         return "UNKNOWN";
        default:                         return "UNKNOWN";
    }
}

int qnx_top_init(void) {
    int cs;
    history_lock(&cs);
    history_clear_locked();
    history_unlock(cs);

#ifdef __QNX__
    /* Elevate thread to real-time priority (Round-Robin) to prevent monitor starvation */
    struct sched_param param;
    memset(&param, 0, sizeof(param));
    param.sched_priority = 25;
    if (sched_setscheduler(0, SCHED_RR, &param) == -1) {
        perror("[qnx-top] sched_setscheduler");
    }
    printf("[qnx-top] Initialized on native QNX Neutrino 8.0 kernel.\n");
#else
    printf("[qnx-top] Initialized in Host Compatibility Mode (simulated data).\n");
#endif
    return 0;
}

/* ------------------------------------------------------------------------- */
/* Native QNX collector                                                      */
/* ------------------------------------------------------------------------- */
#ifdef __QNX__

#define MAX_WAITS     512
#define MAX_RECEIVERS 1024

/* A client thread REPLY-blocked on a server channel. */
typedef struct {
    int server_pid;
    int server_chid;
    float stall_ms;
} reply_wait_t;

/* A server thread RECEIVE-blocked on one of its channels. */
typedef struct {
    int pid;
    int tid;
    int priority;
    int chid;
} receiver_t;

static qnx_thread_state_t map_qnx_state(unsigned state) {
    switch (state) {
#ifdef STATE_DEAD
        case STATE_DEAD:        return QNX_THREAD_DEAD;
#endif
#ifdef STATE_RUNNING
        case STATE_RUNNING:     return QNX_THREAD_RUNNING;
#endif
#ifdef STATE_READY
        case STATE_READY:       return QNX_THREAD_READY;
#endif
#ifdef STATE_STOPPED
        case STATE_STOPPED:     return QNX_THREAD_STOPPED;
#endif
#ifdef STATE_SEND
        case STATE_SEND:        return QNX_THREAD_SEND_BLOCKED;
#endif
#ifdef STATE_RECEIVE
        case STATE_RECEIVE:     return QNX_THREAD_RECEIVE_BLOCKED;
#endif
#ifdef STATE_REPLY
        case STATE_REPLY:       return QNX_THREAD_REPLY_BLOCKED;
#endif
#ifdef STATE_STACK
        case STATE_STACK:       return QNX_THREAD_STACK;
#endif
#ifdef STATE_WAITPAGE
        case STATE_WAITPAGE:    return QNX_THREAD_WAITPAGE;
#endif
#ifdef STATE_NANOSLEEP
        case STATE_NANOSLEEP:   return QNX_THREAD_NANOSLEEP;
#endif
#ifdef STATE_MUTEX
        case STATE_MUTEX:       return QNX_THREAD_MUTEX;
#endif
#ifdef STATE_CONDVAR
        case STATE_CONDVAR:     return QNX_THREAD_CONDVAR;
#endif
#ifdef STATE_JOIN
        case STATE_JOIN:        return QNX_THREAD_JOIN;
#endif
#ifdef STATE_INTR
        case STATE_INTR:        return QNX_THREAD_INTR;
#endif
#ifdef STATE_SEM
        case STATE_SEM:         return QNX_THREAD_SEM;
#endif
#ifdef STATE_WAITCTX
        case STATE_WAITCTX:     return QNX_THREAD_WAITCTX;
#endif
#ifdef STATE_NET_SEND
        case STATE_NET_SEND:    return QNX_THREAD_NET_SEND;
#endif
#ifdef STATE_NET_REPLY
        case STATE_NET_REPLY:   return QNX_THREAD_NET_REPLY;
#endif
        default:                return QNX_THREAD_UNKNOWN;
    }
}

/* Executable path of the process, via DCMD_PROC_MAPDEBUG_BASE. */
static void read_process_name(int fd, int pid, char *out, size_t out_size) {
    union {
        procfs_debuginfo info;
        char raw[sizeof(procfs_debuginfo) + PATH_MAX];
    } map;
    memset(&map, 0, sizeof(map));

    if (devctl(fd, DCMD_PROC_MAPDEBUG_BASE, &map, sizeof(map), NULL) == EOK) {
        const char *path = map.raw + offsetof(procfs_debuginfo, path);
        if (path[0] != '\0') {
            copy_str(out, out_size, path);
            return;
        }
    }
    snprintf(out, out_size, "[pid %d]", pid);
}

static void collect_memory(qnx_system_snapshot_t *snapshot) {
#if defined(_SC_PHYS_PAGES) && defined(_SC_PAGESIZE)
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    if (pages > 0 && page_size > 0) {
        uint64_t mib = ((uint64_t)pages * (uint64_t)page_size) / (1024ULL * 1024ULL);
        snapshot->ram_total_mb = mib;
        snapshot->avail.ram_total = true;
#ifdef _SC_AVPHYS_PAGES
        long free_pages = sysconf(_SC_AVPHYS_PAGES);
        if (free_pages >= 0 && free_pages <= pages) {
            snapshot->ram_used_mb = ((uint64_t)(pages - free_pages) * (uint64_t)page_size) /
                                    (1024ULL * 1024ULL);
            snapshot->avail.ram_used = true;
        }
#endif
    }
#else
    (void)snapshot;
#endif
}

/* true if the Adaptive Partitioning scheduler is present on the running kernel. */
static bool probe_aps_active(void) {
#if defined(SCHED_APS_QUERY_PARTITION)
    sched_aps_partition_info info;
#ifdef APS_INIT_DATA
    APS_INIT_DATA(&info);
#else
    memset(&info, 0, sizeof(info));
#endif
    info.id = 0; /* the System partition always exists when APS is present */
    return SchedCtl(SCHED_APS_QUERY_PARTITION, &info, sizeof(info)) == EOK;
#else
    return false;
#endif
}

static int find_process_index(const qnx_system_snapshot_t *snapshot, int pid) {
    for (int i = 0; i < snapshot->num_processes; i++) {
        if (snapshot->processes[i].pid == pid) return i;
    }
    return -1;
}

/*
 * Build the channel matrix from observed REPLY waits.
 * Ownership goes to the SERVER (the process hosting the channel named in the
 * client's blocked-on info); the client is only counted as a blocked client.
 */
static void build_channel_matrix(qnx_system_snapshot_t *snapshot,
                                 const reply_wait_t *waits, int num_waits,
                                 const receiver_t *receivers, int num_receivers) {
    int count = 0;

    for (int w = 0; w < num_waits; w++) {
        int idx = -1;
        for (int c = 0; c < count; c++) {
            if (snapshot->channels[c].owner_pid == waits[w].server_pid &&
                snapshot->channels[c].chid == waits[w].server_chid) {
                idx = c;
                break;
            }
        }
        if (idx < 0) {
            if (count >= MAX_CHANNELS) continue;
            idx = count++;
            qnx_channel_info_t *c = &snapshot->channels[idx];
            memset(c, 0, sizeof(*c));
            c->chid = waits[w].server_chid;
            c->owner_pid = waits[w].server_pid;
            c->receive_tid = -1;
            c->receive_priority = -1;

            int pi = find_process_index(snapshot, waits[w].server_pid);
            copy_str(c->owner_name, sizeof(c->owner_name),
                     pi >= 0 ? snapshot->processes[pi].name : "N/A");

            for (int r = 0; r < num_receivers; r++) {
                if (receivers[r].pid == c->owner_pid && receivers[r].chid == c->chid) {
                    c->receive_tid = receivers[r].tid;
                    c->receive_priority = receivers[r].priority;
                    break;
                }
            }
        }

        qnx_channel_info_t *c = &snapshot->channels[idx];
        c->blocked_clients++;
        if (waits[w].stall_ms > c->max_stall_latency_ms) {
            c->max_stall_latency_ms = waits[w].stall_ms;
        }
        c->is_stalled = (c->max_stall_latency_ms > QNX_TOP_STALL_THRESHOLD_MS);
    }
    snapshot->num_channels = count;

    /* Record which channel each server process receives on. */
    for (int r = 0; r < num_receivers; r++) {
        int pi = find_process_index(snapshot, receivers[r].pid);
        if (pi >= 0 && snapshot->processes[pi].active_chid == 0) {
            snapshot->processes[pi].active_chid = receivers[r].chid;
        }
    }
}

/* Called with g_history_mutex held. */
static int collect_native_qnx_snapshot_locked(qnx_system_snapshot_t *snapshot) {
    snapshot->is_native_qnx = true;
    uint64_t now_ns = get_time_now_ns();
    snapshot->timestamp_ns = now_ns;

    /* Availability: only what the native interfaces below really measure. */
    snapshot->avail.core_util = false;   /* needs idle-thread / TraceEvent accounting */
    snapshot->avail.core_ctxsw = false;  /* needs TraceEvent */
    snapshot->avail.pools = false;
    snapshot->avail.msg_rates = false;
    snapshot->avail.pulses = false;
    snapshot->avail.dev_details = false;

    snapshot->num_cores = _syspage_ptr->num_cpu;
    if (snapshot->num_cores > MAX_CORES) snapshot->num_cores = MAX_CORES;
    if (snapshot->num_cores == 0) snapshot->num_cores = 1;

    collect_memory(snapshot);

    reply_wait_t *waits = calloc(MAX_WAITS, sizeof(*waits));
    receiver_t *receivers = calloc(MAX_RECEIVERS, sizeof(*receivers));
    if (!waits || !receivers) {
        free(waits);
        free(receivers);
        return -1;
    }
    int num_waits = 0;
    int num_receivers = 0;

    DIR *dir = opendir("/proc");
    if (!dir) {
        free(waits);
        free(receivers);
        return -1;
    }

    int proc_count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && proc_count < MAX_PROCESSES) {
        char *end = NULL;
        long parsed = strtol(ent->d_name, &end, 10);
        if (end == ent->d_name || *end != '\0' || parsed <= 0) continue;
        int pid = (int)parsed;

        char path[64];
        snprintf(path, sizeof(path), "/proc/%d/as", pid);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;

        procfs_info info;
        memset(&info, 0, sizeof(info));
        if (devctl(fd, DCMD_PROC_INFO, &info, sizeof(info), NULL) == EOK) {
            qnx_process_info_t *p = &snapshot->processes[proc_count];
            memset(p, 0, sizeof(*p));
            p->pid = pid;
            p->ppid = (int)info.parent;
            read_process_name(fd, pid, p->name, sizeof(p->name));

            struct stat st;
            if (fstat(fd, &st) == 0 && st.st_size > 0) {
                p->memory_vsize_bytes = (uint64_t)st.st_size;
            }

            uint64_t sum_delta_sutime = 0;
            uint64_t wall_delta = 0;
            bool any_prev = false;
            int t_idx = 0;
            int next_tid = 1;

            while (t_idx < MAX_THREADS_PER_PROC) {
                procfs_status status;
                memset(&status, 0, sizeof(status));
                status.tid = (pthread_t)next_tid;

                /* Returns the first thread with tid >= the requested one. */
                if (devctl(fd, DCMD_PROC_TIDSTATUS, &status, sizeof(status), NULL) != EOK) break;
                int tid = (int)status.tid;
                if (tid < next_tid) break;
                next_tid = tid + 1;

                bool in_reply = (map_qnx_state(status.state) == QNX_THREAD_REPLY_BLOCKED);

                history_obs_t obs;
                history_observe_locked(pid, tid, (uint64_t)status.sutime, in_reply, now_ns, &obs);
                if (obs.had_prev) {
                    any_prev = true;
                    sum_delta_sutime += obs.delta_sutime_ns;
                    if (obs.delta_wall_ns > wall_delta) wall_delta = obs.delta_wall_ns;
                }

                qnx_thread_info_t *t = &p->threads[t_idx++];
                t->tid = tid;
                t->priority = (int)status.priority;
                t->policy = (int)status.policy;
                t->state = map_qnx_state(status.state);
                t->cpu_time_ns = (uint64_t)status.sutime;
                t->reply_stall_ms = obs.stall_ms;

                if (in_reply) {
                    /* For STATE_REPLY, blocked.connect names the SERVER's pid and channel. */
                    t->blocked_on_pid = (int)status.blocked.connect.pid;
                    t->blocked_on_chid = (int)status.blocked.connect.chid;
                    if (num_waits < MAX_WAITS) {
                        waits[num_waits].server_pid = t->blocked_on_pid;
                        waits[num_waits].server_chid = t->blocked_on_chid;
                        waits[num_waits].stall_ms = obs.stall_ms;
                        num_waits++;
                    }
                } else if (t->state == QNX_THREAD_RECEIVE_BLOCKED && num_receivers < MAX_RECEIVERS) {
                    receivers[num_receivers].pid = pid;
                    receivers[num_receivers].tid = tid;
                    receivers[num_receivers].priority = t->priority;
                    receivers[num_receivers].chid = (int)status.blocked.channel.chid;
                    num_receivers++;
                }
            }
            p->num_threads = t_idx;

            if (any_prev && wall_delta > 0) {
                float cpu = ((float)sum_delta_sutime / (float)wall_delta) * 100.0f /
                            (float)snapshot->num_cores;
                if (cpu > 100.0f) cpu = 100.0f;
                p->cpu_percent = cpu;
                p->cpu_valid = true;
            }

            proc_count++;
        }
        close(fd);
    }
    closedir(dir);
    snapshot->num_processes = proc_count;

    history_sweep_locked();

    build_channel_matrix(snapshot, waits, num_waits, receivers, num_receivers);
    free(waits);
    free(receivers);

    /* Adaptive Partitioning: probe only; per-partition budgets are not read. */
    snapshot->aps_active = probe_aps_active();
    snapshot->num_partitions = 0;

    /* /dev directory entries. Owner/handler introspection is not implemented. */
    DIR *devdir = opendir("/dev");
    int dev_count = 0;
    if (devdir) {
        struct dirent *devent;
        while ((devent = readdir(devdir)) != NULL && dev_count < MAX_DEV_NODES) {
            if (devent->d_name[0] == '.') continue;
            qnx_resmgr_node_t *n = &snapshot->dev_nodes[dev_count++];
            memset(n, 0, sizeof(*n));
            snprintf(n->path, sizeof(n->path), "/dev/%.*s",
                     (int)(sizeof(n->path) - 6), devent->d_name);
            n->owner_pid = -1;
            copy_str(n->owner_name, sizeof(n->owner_name), "N/A");
            copy_str(n->handlers, sizeof(n->handlers), "N/A");
        }
        closedir(devdir);
    }
    snapshot->num_dev_nodes = dev_count;

    return 0;
}

#else /* !__QNX__ */

/* ------------------------------------------------------------------------- */
/* Host simulator (clearly labelled SIMULATED by is_native_qnx == false)      */
/* ------------------------------------------------------------------------- */

static void sim_thread(qnx_process_info_t *p, int tid, int prio, int policy,
                       qnx_thread_state_t state, int blocked_pid, int blocked_chid,
                       float stall_ms) {
    qnx_thread_info_t *t = &p->threads[p->num_threads++];
    t->tid = tid;
    t->priority = prio;
    t->policy = policy;
    t->state = state;
    t->blocked_on_pid = blocked_pid;
    t->blocked_on_chid = blocked_chid;
    t->reply_stall_ms = stall_ms;
}

static qnx_process_info_t *sim_process(qnx_system_snapshot_t *s, int pid, int ppid,
                                       const char *name, uint64_t vsize, float cpu,
                                       int active_chid, uint64_t pulses) {
    qnx_process_info_t *p = &s->processes[s->num_processes++];
    p->pid = pid;
    p->ppid = ppid;
    copy_str(p->name, sizeof(p->name), name);
    p->memory_vsize_bytes = vsize;
    p->cpu_percent = cpu;
    p->cpu_valid = true;
    p->active_chid = active_chid;
    p->total_pulses_received = pulses;
    return p;
}

static void sim_channel(qnx_system_snapshot_t *s, int chid, int owner_pid, const char *owner,
                        int rx_tid, int rx_prio, int clients, uint64_t msgs, bool stalled,
                        float stall_ms) {
    qnx_channel_info_t *c = &s->channels[s->num_channels++];
    c->chid = chid;
    c->owner_pid = owner_pid;
    copy_str(c->owner_name, sizeof(c->owner_name), owner);
    c->receive_tid = rx_tid;
    c->receive_priority = rx_prio;
    c->blocked_clients = clients;
    c->messages_per_sec = msgs;
    c->is_stalled = stalled;
    c->max_stall_latency_ms = stall_ms;
}

static void sim_partition(qnx_system_snapshot_t *s, const char *name, float budget,
                          float current, float peak, uint64_t jitter_us, bool critical) {
    qnx_aps_partition_t *a = &s->partitions[s->num_partitions++];
    copy_str(a->name, sizeof(a->name), name);
    a->budget_percent = budget;
    a->current_usage_percent = current;
    a->peak_usage_percent = peak;
    a->max_jitter_us = jitter_us;
    a->is_critical = critical;
}

static void sim_dev_node(qnx_system_snapshot_t *s, const char *path, int owner_pid,
                         const char *owner, int major, int minor, const char *handlers,
                         uint64_t reqs) {
    qnx_resmgr_node_t *n = &s->dev_nodes[s->num_dev_nodes++];
    copy_str(n->path, sizeof(n->path), path);
    n->owner_pid = owner_pid;
    copy_str(n->owner_name, sizeof(n->owner_name), owner);
    n->major = major;
    n->minor = minor;
    copy_str(n->handlers, sizeof(n->handlers), handlers);
    n->io_requests_per_sec = reqs;
}

static int collect_simulated_snapshot(qnx_system_snapshot_t *snapshot) {
    snapshot->is_native_qnx = false;
    snapshot->timestamp_ns = get_time_now_ns();

    /* Simulated data: every metric is "available" so the UI can be demonstrated. */
    snapshot->avail.core_util = true;
    snapshot->avail.core_ctxsw = true;
    snapshot->avail.ram_total = true;
    snapshot->avail.ram_used = true;
    snapshot->avail.pools = true;
    snapshot->avail.msg_rates = true;
    snapshot->avail.pulses = true;
    snapshot->avail.dev_details = true;

    snapshot->num_cores = 4;
    snapshot->ram_total_mb = 4096;
    snapshot->ram_used_mb = 812;
    snapshot->resmgr_pool_kb = 48200;
    snapshot->shm_pool_kb = 128000;
    snapshot->total_channel_msgs_sec = 97420;

    snapshot->core_utilization[0] = 64.2f;
    snapshot->core_utilization[1] = 32.1f;
    snapshot->core_utilization[2] = 51.4f;
    snapshot->core_utilization[3] = 14.8f;
    snapshot->core_ctxsw_per_sec[0] = 3920;
    snapshot->core_ctxsw_per_sec[1] = 2140;
    snapshot->core_ctxsw_per_sec[2] = 3310;
    snapshot->core_ctxsw_per_sec[3] = 1020;

    qnx_process_info_t *p;
    p = sim_process(snapshot, 1, 0, "[kernel/procmgr]", 2400000, 0.8f, 0x01, 2410);
    sim_thread(p, 1, 255, 0, QNX_THREAD_RUNNING, 0, 0, 0.0f);
    sim_thread(p, 2, 250, 0, QNX_THREAD_RECEIVE_BLOCKED, 0, 0, 0.0f);
    p = sim_process(snapshot, 124, 1, "/sbin/devc-ser8250", 16000000, 2.4f, 0x03, 84102);
    sim_thread(p, 1, 21, 0, QNX_THREAD_RECEIVE_BLOCKED, 0, 0, 0.0f);
    p = sim_process(snapshot, 256, 1, "/sbin/io-pkt-v6-hc", 32000000, 4.1f, 0x04, 12940);
    sim_thread(p, 1, 30, 0, QNX_THREAD_RECEIVE_BLOCKED, 0, 0, 0.0f);
    sim_thread(p, 2, 28, 0, QNX_THREAD_READY, 0, 0, 0.0f);
    p = sim_process(snapshot, 312, 1, "/sbin/devb-eide", 12000000, 1.2f, 0, 312);
    sim_thread(p, 1, 18, 0, QNX_THREAD_REPLY_BLOCKED, 124, 0x03, 12.4f);
    p = sim_process(snapshot, 480, 1, "/usr/bin/safety_actuator", 48000000, 14.2f, 0x08, 450119);
    sim_thread(p, 1, 40, 1, QNX_THREAD_RUNNING, 0, 0, 0.0f);
    p = sim_process(snapshot, 512, 1, "/usr/bin/can_gateway", 24000000, 1.9f, 0x05, 4200);
    sim_thread(p, 1, 22, 0, QNX_THREAD_RECEIVE_BLOCKED, 0, 0, 0.0f);

    sim_channel(snapshot, 0x01, 1, "procmgr", 2, 250, 0, 12400, false, 0.0f);
    sim_channel(snapshot, 0x03, 124, "devc-ser8250", 1, 21, 1, 1820, true, 12.4f);
    sim_channel(snapshot, 0x04, 256, "io-pkt-v6-hc", 1, 30, 0, 28950, false, 0.0f);
    sim_channel(snapshot, 0x08, 480, "safety_actuator", 1, 40, 0, 50000, false, 0.0f);

    snapshot->aps_active = true;
    sim_partition(snapshot, "System_Kernel", 10.0f, 8.5f, 9.8f, 5, true);
    sim_partition(snapshot, "Safety_Critical", 65.0f, 48.2f, 62.4f, 12, true);
    sim_partition(snapshot, "Networking_IO", 15.0f, 14.1f, 14.9f, 35, false);
    sim_partition(snapshot, "Background_Telemetry", 10.0f, 6.2f, 8.9f, 80, false);

    sim_dev_node(snapshot, "/dev/qnx_top", 819, "qnx-top-agent", 10, 0, "_IO_READ, _IO_DEVCTL", 1520);
    sim_dev_node(snapshot, "/dev/ser1", 124, "devc-ser8250", 4, 64, "_IO_READ, _IO_DEVCTL", 1420);
    sim_dev_node(snapshot, "/dev/socket", 256, "io-pkt", 8, 0, "_IO_READ, _IO_WRITE, _IO_DEVCTL", 18910);
    sim_dev_node(snapshot, "/dev/can0", 512, "can_gateway", 12, 0, "_IO_DEVCTL, _IO_READ", 4100);
    sim_dev_node(snapshot, "/dev/safety_actuator", 480, "safety_actuator", 14, 0, "_IO_DEVCTL, _IO_PULSE", 50000);

    return 0;
}
#endif /* __QNX__ */

int qnx_top_collect_snapshot(qnx_system_snapshot_t *snapshot) {
    if (!snapshot) return -1;

    int rc;
    int cs;
    history_lock(&cs);              /* single lock/unlock pair: no early returns between */
    memset(snapshot, 0, sizeof(*snapshot));
#ifdef __QNX__
    rc = collect_native_qnx_snapshot_locked(snapshot);
#else
    rc = collect_simulated_snapshot(snapshot);
#endif
    history_unlock(cs);
    return rc;
}

void qnx_top_cleanup(void) {
    printf("[qnx-top] Shutdown clean.\n");
}
