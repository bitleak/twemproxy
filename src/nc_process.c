#include <signal.h>
#include <nc_conf.h>
#include <nc_process.h>

// Global process management states. TODO: set those flags in signal handlers
bool pm_reload = false;
bool pm_respawn = false;

// Master process's jobs:
//   1. reload conf
//   2. diff old listening sockets from new, and close outdated sockets
//   3. bind listening sockets for all workers
//   4. spawn workers
//   5. loop for signals
rstatus_t
nc_multi_processes_cycle(struct instance *nci) {
    rstatus_t status;

    pm_respawn = true; // spawn workers upon start

    while (true) {
        if (pm_reload) {
            // TODO: reload config
        }

        if (pm_respawn) {
            status = nc_spawn_workers(nci->ctx->cf->global.worker_processes, nci);
            if (status != NC_OK) {
                break;
            }
            pm_respawn = false;
        }

        sigsuspend(0); // wake when signal arrives. TODO: add timer using setitimer
    }
    return status;
}

rstatus_t
nc_spawn_workers(int n, struct instance *nci) {
    pid_t pid;

    for (int i = 0; i < n; ++i) {
        switch (pid = fork()) {
        case -1:
            log_error("failed to spawn worker");
            return NC_ERROR;
        case 0:
            // TODO: setup the communication channel between master and workers
            return nc_worker_process(i, nci);
        default:
            log_debug(LOG_NOTICE, "worker [%d] started", pid);
            break;
        }
    }
    return NC_OK;
}

rstatus_t
nc_worker_process(int worker_id, struct instance *nci) {
    rstatus_t status;

    status = core_init_listener(nci);
    if (status != NC_OK) {
        return status;
    }
    status = core_init_instance(nci);
    if (status != NC_OK) {
        return status;
    }

    // TODO: add master/workers communication channel to event base

    while (true) {
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }
    return status;
}

rstatus_t
nc_single_process_cycle(struct instance *nci) {
    rstatus_t status;

    status = core_init_listener(nci);
    if (status != NC_OK) {
        return status;
    }
    status = core_init_instance(nci);
    if (status != NC_OK) {
        return status;
    }

    while (true) {
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }
    return status;
}
