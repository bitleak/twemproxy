#include <sys/mman.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <grp.h>

#include <nc_conf.h>
#include <nc_process.h>
#include <nc_proxy.h>


static rstatus_t nc_migrate_proxies(struct context *dst, struct context *src);
static rstatus_t nc_setup_listener_for_workers(struct instance *parent_nci, bool reloading);
static rstatus_t nc_spawn_workers(struct array *workers);
static void      nc_worker_process(int worker_id, struct instance *nci);
static rstatus_t nc_shutdown_workers(struct array *workers);

// Global process management states.
bool pm_reload = false;
bool pm_respawn = false;
char pm_myrole = ROLE_MASTER;
bool pm_quit = false; // quit right away
bool pm_terminate= false; // quit after worker_shutdown_timeout

struct instance *master_nci = NULL;

static rstatus_t
nc_clone_instance(int worker_id, struct instance *dst, struct instance *src)
{
    struct context *new_ctx;
    if (dst == NULL || src == NULL) {
        return NC_ERROR;
    }
    nc_memcpy(dst, src, sizeof(struct instance));
    dst->id = worker_id;
    new_ctx = core_ctx_create(dst);
    if (new_ctx == NULL) {
        log_error("failed to create context");
        return NC_ERROR;
    }

    new_ctx->shared_mem = nc_shared_mem_alloc(SHARED_MEMORY_SIZE);
    if (new_ctx->shared_mem == NULL)  {
        log_error("failed to create shared memory for context");
        return  NC_ERROR;
    }

    dst->ctx = new_ctx;
    return NC_OK;
}

static rstatus_t
nc_each_close_other_proxy(void *elem, void *data)
{
    struct instance *nci = (struct instance *)elem, *self = data;
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
    return array_each(workers, nc_each_close_other_proxy, self);
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
    struct context *ctx, *prev_ctx;
    sigset_t set;

    status = core_init_stats(parent_nci);
    if (status != NC_OK) {
        return status;
    }

    pm_respawn = true; // spawn workers upon start
    status = nc_setup_listener_for_workers(parent_nci, false);
    if (status != NC_OK) {
        log_error("[master] failed to setup listeners");
        return status;
    }

    for (;;) {
        if (pm_reload) {
            pm_reload = false; // restart workers
            log_warn("reloading config");
            ctx = core_ctx_create(parent_nci);
            if (ctx == NULL) {
                log_error("[master] failed to recreate context");
                continue;
            }
            prev_ctx = parent_nci->ctx;
            ctx->stats = prev_ctx->stats;
            parent_nci->ctx = ctx;

            status = nc_setup_listener_for_workers(parent_nci, true);
            if (status != NC_OK) {
                // skip reloading
                parent_nci->ctx = prev_ctx;
                ctx->stats = NULL;
                core_ctx_destroy(ctx);
                continue;
            }
            prev_ctx->stats = NULL;
            core_ctx_destroy(prev_ctx);
            pm_respawn = true; // restart workers
        }

        if (pm_respawn) {
            pm_respawn = false;
            status = nc_spawn_workers(&parent_nci->workers);
            if (status != NC_OK) {
                break;
            }
        }

        sigemptyset(&set);
        sigsuspend(&set); // wake when signal arrives.
    }
    return status;
}

static rstatus_t
nc_setup_listener_for_workers(struct instance *parent_nci, bool reloading)
{
    rstatus_t status;
    int i, n = parent_nci->ctx->cf->global.worker_processes;
    int old_workers_n = 0;
    struct instance *worker_nci, *old_worker_nci;
    struct array old_workers;

    if (reloading) {
        old_workers = parent_nci->workers;
        old_workers_n = (int)array_n(&old_workers);
    }

    status = array_init(&parent_nci->workers, (uint32_t)n, sizeof(struct instance));
    if (status != NC_OK) {
        log_error("failed to init parent_nci->workers, rollback");
        parent_nci->workers.nelem = 0;
        goto rollback_step1;
    }

    for (i = 0; i < n; i++) {
        worker_nci = array_push(&parent_nci->workers);
        status = nc_clone_instance(i, worker_nci, parent_nci);
        if (status != NC_OK) {
            log_error("failed to clone parent_nci, rollback");
            goto rollback_step2;
        }
        worker_nci->role = ROLE_WORKER;
    }

    for (i = 0; i < n; i++) {
        worker_nci = array_get(&parent_nci->workers, (uint32_t)i);
        if (reloading && i < old_workers_n) {
            old_worker_nci = array_get(&old_workers, (uint32_t)i);
            nc_migrate_proxies(worker_nci->ctx, old_worker_nci->ctx);
        }

        status = proxy_init(worker_nci->ctx); // will skip some proxy listeners if they are migrated (p_conn != NULL)
        if (status != NC_OK) {
            log_error("failed to init worker's listener, rollback");
            goto rollback_step3;
        }
    }
    if (reloading) {
        nc_shutdown_workers(&old_workers);
    }

    return NC_OK;

rollback_step3:
    if (!reloading) {
        return status;
    }
    for(i = 0; i < old_workers_n && i < n; i++) {
        // restore old_worker_nci'proxies
        worker_nci = array_get(&parent_nci->workers, (uint32_t)i);
        old_worker_nci = array_get(&old_workers, (uint32_t)i);
        nc_migrate_proxies(old_worker_nci->ctx, worker_nci->ctx);
    }

rollback_step2:
    if (!reloading) {
        return status;
    }
    for(i = 0; i < (int)array_n(&parent_nci->workers); i++) {
        // destroy new worker_nci's ctx
        worker_nci = array_pop(&parent_nci->workers);
        core_ctx_destroy(worker_nci->ctx);
    }

rollback_step1:
    if (!reloading) {
        return status;
    }
    array_deinit(&parent_nci->workers);
    parent_nci->workers = old_workers;
    return status;
}

static rstatus_t
nc_spawn_worker(int worker_id, struct instance *worker_nci, struct array *workers) {
    pid_t pid;

    worker_nci->chan = nc_alloc_channel();
    if (worker_nci->chan == NULL) {
        return NC_ENOMEM;
    }

    switch (pid = fork()) {
    case -1:
        return NC_ERROR;
    case 0:
        pm_myrole = ROLE_WORKER;
        pid = getpid();
        worker_nci->pid = pid;
        nc_close_other_proxies(workers, worker_nci);
        nc_worker_process(worker_id, worker_nci);
        NOT_REACHED();
    default:
        worker_nci->pid = pid;
        log_warn("worker [%d] started", pid);
        break;
    }
    return NC_OK;
}

static rstatus_t
nc_spawn_workers(struct array *workers)
{
    int i;
    struct instance *worker_nci;

    ASSERT(array_n(workers) > 0);

    for (i = 0; (uint32_t)i < array_n(workers); ++i) {
        worker_nci = (struct instance *)array_get(workers, (uint32_t)i);
        if (nc_spawn_worker(i, worker_nci, workers) != NC_OK) {
            log_error("failed to spawn worker");
        }
    }
    return NC_OK;
}

static rstatus_t
nc_shutdown_workers(struct array *workers)
{
    uint32_t i, nelem;
    void *elem;
    struct instance *worker_nci;

    nc_signal_workers(workers, NC_CMD_TERMINATE);
    for (i = 0, nelem = array_n(workers); i < nelem; i++) {
        elem = array_pop(workers);
        worker_nci = (struct instance *)elem;
        nc_dealloc_channel(worker_nci->chan);
        core_ctx_destroy(worker_nci->ctx);
    }
    array_deinit(workers);
    return NC_OK;
}

static void
nc_worker_process(int worker_id, struct instance *nci)
{
    rstatus_t status;
    sigset_t set;
    struct conf *cf = nci->ctx->cf;

    ASSERT(nci->role == ROLE_WORKER);

    if (geteuid() == 0) {
        if (setgid(cf->global.gid) == -1) {
            log_error("failed to setgid");
            exit(0);
        }

        if (initgroups((char *)cf->global.user.data, (int)cf->global.gid) == -1) {
            log_error("failed to initgroups");
        }

        if (setuid(cf->global.uid) == -1) {
            log_error("failed to setuid");
            exit(0);
        }
    }

    sigemptyset(&set);
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        log_error("failed to clear signal mask");
        return;
    }

    status = core_init_stats(nci);
    if (status != NC_OK) {
        log_error("failed to initialize stats");
        return;
    }
    // Close inherited stats listening FD
    close(master_nci->ctx->stats->sd);

    status = core_init_instance(nci);
    if (status != NC_OK) {
        log_error("failed to initialize");
        return;
    }

    status = nc_add_channel_event(nci->ctx->evb, nci->chan->fds[1]);
    if (status != NC_OK) {
        log_error("failed to add channel event");
        return;
    }

    bool terminating = false;
    for (;!pm_quit;) {
        if (pm_terminate && !terminating) {
            // close proxy listen fd, and wait for 30 seconds
            array_each(&nci->ctx->pool, proxy_each_unaccept, NULL);
            nc_set_timer(nci->ctx->cf->global.worker_shutdown_timeout * 1000, 0);
            terminating = true;
        }
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }
    core_ctx_destroy(nci->ctx);

    log_warn("[worker] terminted with quit flag: %d", pm_quit);

    exit(0);
}

rstatus_t
nc_single_process_cycle(struct instance *nci)
{
    rstatus_t status;

    status = core_init_stats(nci);
    if (status != NC_OK) {
        return status;
    }

    status = core_init_listener(nci);
    if (status != NC_OK) {
        return status;
    }

    status = core_init_instance(nci);
    if (status != NC_OK) {
        return status;
    }

    for (;;) {
        status = core_loop(nci->ctx);
        if (status != NC_OK) {
            break;
        }
    }
    return status;
}

void
nc_reload_config(void)
{
    pm_reload = true;
}

void
nc_reap_worker(void)
{
    uint32_t i, nelem;
    int status;
    pid_t pid;
    int err;
    struct instance *worker_nci;

    ASSERT(master_nci != NULL);

    for ( ;; ) {
        pid = waitpid(-1, &status, WNOHANG);

        if (pid == 0) {
            return;
        }

        if (pid == -1) {
            err = errno;

            if (err == EINTR) {
                continue;
            }

            if (err == ECHILD) {
                return;
            }
        }

        if (WIFEXITED(status)) {
            log_warn("worker [%d] exited with status: %d", pid, WEXITSTATUS(status));
            if (WEXITSTATUS(status) == 0) {
                // worker shutdown due to config reloading, we don't need to respawn worker or cleanup ends here
                continue;
            }
        }
        if (WIFSIGNALED(status)) {
            log_warn("worker [%d] terminated", pid);
        }

        for (i = 0, nelem = array_n(&master_nci->workers); i < nelem; i++) {
            worker_nci = (struct instance*)array_get(&master_nci->workers, i);
            if (worker_nci->pid == pid) {
                log_debug(LOG_NOTICE, "respawn worker to replace [%d]", pid);
                nc_dealloc_channel(worker_nci->chan);
                nc_spawn_worker((int)i, worker_nci, &master_nci->workers); // spawn worker use old ctx which is enough
            }
        }
    }
}

// keep the src (old) context's proxies if they exist in the dst (new) context
static rstatus_t
nc_migrate_proxies(struct context *dst, struct context *src)
{
    uint32_t i, nelem, j, nelem2;
    void *elem;
    struct array *src_pools = &src->pool;
    struct array *dst_pools = &dst->pool;
    struct string *src_proxy_name, *src_proxy_addrstr;
    struct string *dst_proxy_name, *dst_proxy_addrstr;
    struct server_pool *src_pool, *dst_pool;

    ASSERT(array_n(src_pools) != 0);
    ASSERT(array_n(dst_pools) != 0);

    for (i = 0, nelem = array_n(src_pools); i < nelem; i++) {
        elem = array_get(src_pools, i);
        src_pool = (struct server_pool *)elem;
        src_proxy_name = &src_pool->name;
        src_proxy_addrstr = &src_pool->addrstr;
        for (j = 0, nelem2 = array_n(dst_pools); j < nelem2; j++) {
            elem = array_get(dst_pools, j);
            dst_pool = (struct server_pool *)elem;
            dst_proxy_name = &dst_pool->name;
            dst_proxy_addrstr = &dst_pool->addrstr;
            if (string_compare(dst_proxy_addrstr, src_proxy_addrstr) == 0) {
                if (string_compare(dst_proxy_name, src_proxy_name) != 0) {
                    log_warn("listening socket's name change from [%s] to [%s]", src_proxy_name->data,
                              dst_proxy_name->data);
                }
                if (dst_pool->p_conn != NULL) {
                    continue;
                }
                log_warn("migrate from [%s] [%s]", src_proxy_name->data, src_proxy_addrstr->data);
                dst_pool->p_conn = src_pool->p_conn;
                dst_pool->p_conn->owner = dst_pool;
                src_pool->p_conn = NULL; // p_conn is migrated, so clear the src_pool
            }
        }
    }
    return NC_OK;
}

void
nc_signal_workers(struct array *workers, int command)
{
    uint32_t i, nelem;
    void *elem;
    struct chan_msg msg;
    struct instance *worker_nci;

    for (i = 0, nelem = array_n(workers); i < nelem; i++) {
        elem = array_get(workers, i);
        worker_nci = (struct instance *)elem;
        msg.command = command;
        if (nc_write_channel(worker_nci->chan->fds[0], &msg) <= 0) {
            log_error("failed to write channel, err %s", strerror(errno));
        }
    }
}
