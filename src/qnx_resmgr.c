/**
 * @file qnx_resmgr.c
 * @brief Exposes /dev/qnx_top as a native QNX 8.0 Resource Manager
 * 
 * Allows any local or remote tool to read live telemetry via standard POSIX open("/dev/qnx_top")
 */

#include "qnx_top.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef __QNX__
#include <sys/iofunc.h>
#include <sys/dispatch.h>
#include <sys/resmgr.h>

static resmgr_connect_funcs_t connect_funcs;
static resmgr_io_funcs_t      io_funcs;
static iofunc_attr_t          attr;

static int io_read_handler(resmgr_context_t *ctp, io_read_t *msg, RESMGR_OCB_T *ocb) {
    qnx_system_snapshot_t snapshot;
    qnx_top_collect_snapshot(&snapshot);

    char buffer[2048];
    int len = snprintf(buffer, sizeof(buffer),
        "QNX-TOP v1.0.0 Microkernel Telemetry Snapshot\n"
        "Cores: %u | RAM: %luMB / %luMB\n"
        "Active Processes: %d | Active Channels: %d | Partitions: %d\n",
        snapshot.num_cores, (unsigned long)snapshot.ram_used_mb, (unsigned long)snapshot.ram_total_mb,
        snapshot.num_processes, snapshot.num_channels, snapshot.num_partitions);

    int nbytes = msg->i.nbytes;
    if (len > nbytes) len = nbytes;

    _IO_SET_READ_NBYTES(ctp, len);
    return resmgr_msgwrite(ctp, buffer, len, 0);
}

int qnx_top_start_resmgr(void) {
    dispatch_t *dpp = dispatch_create();
    if (!dpp) {
        perror("dispatch_create");
        return -1;
    }

    resmgr_attr_t resmgr_attr;
    memset(&resmgr_attr, 0, sizeof(resmgr_attr));
    resmgr_attr.nparts_max = 1;
    resmgr_attr.msg_max_size = 2048;

    iofunc_func_init(_RESMGR_CONNECT_NFUNCS, &connect_funcs,
                     _RESMGR_IO_NFUNCS, &io_funcs);
    io_funcs.read = io_read_handler;

    iofunc_attr_init(&attr, S_IFCHR | 0666, 0, 0);

    int resmgr_id = resmgr_attach(dpp, &resmgr_attr, "/dev/qnx_top", _FTYPE_ANY,
                                  0, &connect_funcs, &io_funcs, &attr);
    if (resmgr_id == -1) {
        perror("resmgr_attach");
        return -1;
    }

    printf("[qnx-top] Successfully registered QNX Resource Manager at /dev/qnx_top\n");
    return 0;
}
#else
int qnx_top_start_resmgr(void) {
    printf("[qnx-top] Resource Manager /dev/qnx_top simulated (Host mode).\n");
    return 0;
}
#endif
