#include <signal.h>
#include <nc_conf.h>
#include <nc_process.h>
#include <nc_proxy.h>

// Global process management states. TODO: set those flags in signal handlers
bool pm_reload = false;
bool pm_respawn = false;

static struct instance *
nc_clone_instance(struct instance *parent_nci)
{
    struct context *new_ctx;
    struct instance *cloned_nci;
    cloned_nci = nc_alloc(sizeof(*parent_nci));
    if (cloned_nci == NULL) {
        return NULL;
    }
    nc_memcpy(cloned_nci, parent_nci, sizeof(*parent_nci));
    new_ctx = core_ctx_create(cloned_nci);
    if (new_ctx == NULL) {
        log_error("failed to create context");
        return NULL;
    }
    cloned_nci->ctx = new_ctx;
    return cloned_nci;
}

static rstatus_t
nc_close_other_proxy(void *elem, void *data)
{
    struct instance *nci = *(struct instance **) elem, *self = data;
    struct context *ctx = nci->ctx;

    if (nci == self) {
        return NC_OK;
    }

    proxy_deinit(ctx);
    return NC_OK;
}

static rstatus_t
nc_close_other_proxies(struct array *workers, struct instance *self)
{
    return array_each(workers, nc_close_other_proxy, self);
}

// Master process's jobs:
//   1. reload conf
//   2. diff old listening sockets from new, and close outdated sockets
//   3. bind listening sockets for all workers
//   4. spawn workers
//   5. loop for signals
rstatus_t
nc_multi_processes_cycle(struct instance *parent_nci)
{
    rstatus_t status;

    pm_respawn = true; // spawn workers upon start

    for(;;) {
        if (pm_reload) {
            // TODO: reload config
        }

        if (pm_respawn) {
            status = nc_spawn_workers(parent_nci->ctx->cf->global.worker_processes, parent_nci);
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
nc_spawn_workers(int n, struct instance *parent_nci)
{
    rstatus_t status;
    pid_t pid;
    struct instance **nci_elem_ptr, *worker_nci;

    ASSERT(parent_nci->role == ROLE_MASTER);
    ASSERT(n >= 0);

    array_init(&parent_nci->workers, (uint32_t)n, sizeof(struct instance *));

    for (int i = 0; i < n; ++i) {
        worker_nci = nc_clone_instance(parent_nci);
        if (worker_nci == NULL) {
            return NC_ERROR;
        }
        worker_nci->role = ROLE_WORKER;
        nci_elem_ptr = array_push(&parent_nci->workers);
        *nci_elem_ptr = worker_nci;

        // listeners are binded in master process
        status = core_init_listener(worker_nci);
        if (status != NC_OK) {
            return status;
        }

        switch (pid = fork()) {
        case -1:
            log_error("failed to spawn worker");
            return NC_ERROR;
        case 0:
            // TODO: setup the communication channel between master and workers
            pid = getpid();
            worker_nci->pid = pid;
            nc_close_other_proxies(&parent_nci->workers, worker_nci);
            nc_worker_process(i, worker_nci);
            NOT_REACHED();
        default:
            worker_nci->pid = pid;
            log_debug(LOG_NOTICE, "worker [%d] started", pid);
            break;
        }
    }
    return NC_OK;
}

void
nc_worker_process(int worker_id, struct instance *nci)
{
    rstatus_t status;

    ASSERT(nci->role == ROLE_WORKER);

    status = core_init_instance(nci);
    if (status != NC_OK) {
        log_error("[worker] failed to initialize");
        return;
    }

    // TODO: add master/workers communication channel to event base
    // TODO: worker should remove the listening sockets from event base and after lingering connections are exhausted
    // or timeout, quit process.

    for(;;) {
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }

    exit(0);
}

rstatus_t
nc_single_process_cycle(struct instance *nci)
{
    rstatus_t status;

    status = core_init_listener(nci);
    if (status != NC_OK) {
        return status;
    }
    status = core_init_instance(nci);
    if (status != NC_OK) {
        return status;
    }

    for(;;) {
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }
    return status;
}
