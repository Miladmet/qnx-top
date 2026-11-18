/**
 * @file qnx_stall_demo.c
 * @brief Demonstration program creating real QNX IPC REPLY-blocked thread stalls
 *
 * This utility creates a reproducible message-passing bottleneck between a client
 * thread and a server channel. It allows developers and judges to observe how
 * qnx-top detects genuine microkernel REPLY-blocked states, tracks stall latency,
 * and attributes the block to the offending server PID and channel ID (chid).
 *
 * Target: BlackBerry QNX Neutrino 8.0 / QNX Everywhere
 * License: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

#if defined(__unix__) || defined(__APPLE__) || defined(__QNX__)
    #include <unistd.h>
    #include <pthread.h>
    #define HAVE_POSIX_THREADS 1
#endif

#if defined(__QNX__)
    #include <sys/neutrino.h>
    #include <sys/netmgr.h>
#endif

/* IPC message types */
typedef struct {
    uint32_t type;
    uint32_t seq;
    char payload[64];
} demo_msg_t;

typedef struct {
    uint32_t status;
    uint32_t ack_seq;
    char reply_text[64];
} demo_reply_t;

/* Global run flag for clean shutdown */
static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig) {
    (void)sig;
    g_running = 0;
}

#if defined(__QNX__)

static int g_chid = -1;
static int g_coid = -1;
static long g_stall_delay_ms = 50; /* 50ms default delay, triggers >2ms stall alert */

static void* server_thread(void* arg) {
    (void)arg;
    demo_msg_t msg;
    demo_reply_t reply;
    struct _msg_info info;

    printf("[SERVER] Server thread active on chid=%d (PID %d)\n", g_chid, (int)getpid());

    while (g_running) {
        /* Receive message from client */
        int rcvid = MsgReceive(g_chid, &msg, sizeof(msg), &info);
        if (rcvid == -1) {
            if (errno == EINTR) continue;
            break;
        }

        /* If pulse or disconnect, handle accordingly */
        if (rcvid == 0) {
            continue;
        }

        /* Deliberately stall before replying to simulate heavy computation / I/O latency */
        if (g_stall_delay_ms > 0) {
            struct timespec ts;
            ts.tv_sec = g_stall_delay_ms / 1000;
            ts.tv_nsec = (g_stall_delay_ms % 1000) * 1000000L;
            nanosleep(&ts, NULL);
        }

        /* Reply to unblock client */
        reply.status = 0;
        reply.ack_seq = msg.seq;
        snprintf(reply.reply_text, sizeof(reply.reply_text), "ACK seq %u", msg.seq);
        MsgReply(rcvid, EOK, &reply, sizeof(reply));
    }

    return NULL;
}

static void* client_thread(void* arg) {
    (void)arg;
    demo_msg_t msg;
    demo_reply_t reply;
    uint32_t seq = 0;

    printf("[CLIENT] Client thread active, connecting to coid=%d\n", g_coid);

    while (g_running) {
        msg.type = 0x01;
        msg.seq = ++seq;
        snprintf(msg.payload, sizeof(msg.payload), "Request packet %u", seq);

        /*
         * MsgSend blocks until server calls MsgReply.
         * While blocked here, this thread is in STATE_REPLY,
         * which qnx-top flags as a REPLY-blocked stall.
         */
        int status = MsgSend(g_coid, &msg, sizeof(msg), &reply, sizeof(reply));
        if (status == -1) {
            if (errno == EINTR) continue;
            perror("[CLIENT] MsgSend failed");
            break;
        }

        /* Brief yield between cycles */
        struct timespec pause_ts = {0, 10 * 1000000L}; /* 10ms */
        nanosleep(&pause_ts, NULL);
    }

    return NULL;
}

#else

/* Host compatibility emulation */
static pthread_mutex_t g_host_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_host_cond  = PTHREAD_COND_INITIALIZER;
static bool g_host_has_work = false;
static bool g_host_work_done = false;
static long g_stall_delay_ms = 50;

static void* host_server_thread(void* arg) {
    (void)arg;
    printf("[HOST DEMO] Simulated server thread active (Host Emulation Mode)\n");

    while (g_running) {
        pthread_mutex_lock(&g_host_mutex);
        while (!g_host_has_work && g_running) {
            pthread_cond_wait(&g_host_cond, &g_host_mutex);
        }
        if (!g_running) {
            pthread_mutex_unlock(&g_host_mutex);
            break;
        }

        /* Simulate server latency stall */
        struct timespec ts;
        ts.tv_sec = g_stall_delay_ms / 1000;
        ts.tv_nsec = (g_stall_delay_ms % 1000) * 1000000L;
        nanosleep(&ts, NULL);

        g_host_has_work = false;
        g_host_work_done = true;
        pthread_cond_broadcast(&g_host_cond);
        pthread_mutex_unlock(&g_host_mutex);
    }
    return NULL;
}

static void* host_client_thread(void* arg) {
    (void)arg;
    printf("[HOST DEMO] Simulated client thread active (Simulating synchronous stall)\n");

    while (g_running) {
        pthread_mutex_lock(&g_host_mutex);
        g_host_has_work = true;
        g_host_work_done = false;
        pthread_cond_broadcast(&g_host_cond);

        while (!g_host_work_done && g_running) {
            pthread_cond_wait(&g_host_cond, &g_host_mutex);
        }
        pthread_mutex_unlock(&g_host_mutex);

        struct timespec pause_ts = {0, 10 * 1000000L};
        nanosleep(&pause_ts, NULL);
    }
    return NULL;
}

#endif

static void print_usage(const char* prog) {
    printf("Usage: %s [OPTIONS]\n", prog);
    printf("Options:\n");
    printf("  -d <ms>       Stall delay injected by server before MsgReply (default: 50 ms)\n");
    printf("  -h            Show this help text\n\n");
    printf("This program starts a client/server pair that demonstrates QNX Neutrino\n");
    printf("message-passing delays and allows qnx-top to capture live REPLY-blocked stalls.\n");
}

int main(int argc, char* argv[]) {
    long delay_ms = 50;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-d") == 0 && i + 1 < argc) {
            delay_ms = strtol(argv[++i], NULL, 10);
            if (delay_ms < 0) delay_ms = 0;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

#if defined(SIGINT)
    signal(SIGINT, handle_signal);
#endif
#if defined(SIGTERM)
    signal(SIGTERM, handle_signal);
#endif

    printf("======================================================================\n");
    printf(" qnx-stall-demo: Microkernel IPC REPLY-Blocked Generator\n");
    printf(" Configured server reply latency: %ld ms (Threshold: >2.0 ms)\n", delay_ms);
    printf(" Press Ctrl+C to stop.\n");
    printf("======================================================================\n");

#if defined(__QNX__)
    g_stall_delay_ms = delay_ms;

    /* Create channel in private process space */
    g_chid = ChannelCreate(0);
    if (g_chid == -1) {
        perror("ChannelCreate failed");
        return 1;
    }

    /* Attach client connection */
    g_coid = ConnectAttach(0, getpid(), g_chid, _NTO_SIDE_CHANNEL, 0);
    if (g_coid == -1) {
        perror("ConnectAttach failed");
        ChannelDestroy(g_chid);
        return 1;
    }

    pthread_t s_tid, c_tid;
    pthread_create(&s_tid, NULL, server_thread, NULL);
    pthread_create(&c_tid, NULL, client_thread, NULL);

    pthread_join(c_tid, NULL);
    pthread_join(s_tid, NULL);

    ConnectDetach(g_coid);
    ChannelDestroy(g_chid);

#elif defined(HAVE_POSIX_THREADS)
    g_stall_delay_ms = delay_ms;
    pthread_t s_tid, c_tid;
    pthread_create(&s_tid, NULL, host_server_thread, NULL);
    pthread_create(&c_tid, NULL, host_client_thread, NULL);

    pthread_join(c_tid, NULL);
    pthread_join(s_tid, NULL);
#else
    printf("POSIX threads not supported on this platform.\n");
#endif

    printf("\n[qnx-stall-demo] Terminated cleanly.\n");
    return 0;
}
