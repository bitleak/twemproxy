/*
 * twemproxy - A fast and lightweight proxy for memcached protocol.
 * Copyright (C) 2011 Twitter, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdlib.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <nc_core.h>
#include <nc.h>
#include <nc_signal.h>
#include <nc_process.h>

static struct signal signals[] = {
    { SIGUSR1, "SIGUSR1", 0,                 signal_handler },
    { SIGUSR2, "SIGUSR2", 0,                 signal_handler },
    { SIGTTIN, "SIGTTIN", 0,                 signal_handler },
    { SIGTTOU, "SIGTTOU", 0,                 signal_handler },
    { SIGHUP,  "SIGHUP",  0,                 signal_handler },
    { SIGINT,  "SIGINT",  0,                 signal_handler },
    { SIGTERM, "SIGTERM", 0,                 signal_handler },
    { SIGSEGV, "SIGSEGV", (int)SA_RESETHAND, signal_handler },
    { SIGCHLD, "SIGCHLD", 0,                 signal_handler },
    { SIGPIPE, "SIGPIPE", 0,                 SIG_IGN },
    { SIGALRM, "SIGALRM", 0,                 signal_handler },
    { 0,        NULL,     0,                 NULL }
};

rstatus_t
signal_init(void)
{
    struct signal *sig;

    for (sig = signals; sig->signo != 0; sig++) {
        rstatus_t status;
        struct sigaction sa;

        memset(&sa, 0, sizeof(sa));
        sa.sa_handler = sig->handler;
        sa.sa_flags = sig->flags;
        sigemptyset(&sa.sa_mask);

        status = sigaction(sig->signo, &sa, NULL);
        if (status < 0) {
            log_error("sigaction(%s) failed: %s", sig->signame,
                      strerror(errno));
            return NC_ERROR;
        }
    }

    return NC_OK;
}

void
signal_deinit(void)
{
}

void
signal_handler(int signo)
{
    struct signal *sig;
    void (*action)(void);
    char *actionstr;
    bool done;

    for (sig = signals; sig->signo != 0; sig++) {
        if (sig->signo == signo) {
            break;
        }
    }
    ASSERT(sig->signo != 0);

    actionstr = "";
    action = NULL;
    done = false;

    switch (signo) {
    case SIGUSR1:
        actionstr = ", reopening log file";
        action = log_reopen;
        if (pm_myrole == ROLE_MASTER) {
            nc_signal_workers(&master_nci->workers, NC_CMD_LOG_REOPEN);
        }
        break;

    case SIGUSR2:
        break;

    case SIGTTIN:
        actionstr = ", up logging level";
        if (pm_myrole == ROLE_MASTER) {
            nc_signal_workers(&master_nci->workers, NC_CMD_LOG_LEVEL_UP);
        }
        action = log_level_up;
        break;

    case SIGTTOU:
        actionstr = ", down logging level";
        action = log_level_down;
        if (pm_myrole == ROLE_MASTER) {
            nc_signal_workers(&master_nci->workers, NC_CMD_LOG_LEVEL_DOWN);
        }
        break;

    case SIGHUP:
        if (pm_myrole == ROLE_MASTER) {
            actionstr = ", reload config";
            action = nc_reload_config;
        }
        break;

    case SIGINT:
        if (pm_myrole == ROLE_MASTER) {
            nc_signal_workers(&master_nci->workers, NC_CMD_QUIT);
            wait(NULL);
            done = true;
        } else {
            pm_quit = true;
        }
        actionstr = ", exiting";
        break;

    case SIGTERM:
        if (pm_myrole == ROLE_MASTER) {
            nc_signal_workers(&master_nci->workers, NC_CMD_TERMINATE);
            wait(NULL);
            done = true;
        } else {
            pm_terminate = true;
        }
        actionstr = ", terminating";
        break;

    case SIGSEGV:
        log_stacktrace();
        actionstr = ", core dumping";
        raise(SIGSEGV);
        break;
    case SIGALRM:
        if (pm_terminate) {
            pm_quit = true;
            pm_terminate = false;
            actionstr = ", time's up, quit";
        }
        break;

    case SIGCHLD:
        ASSERT(pm_myrole == ROLE_MASTER);
        actionstr = ", reaping child";
        action = nc_reap_worker;
        break;

    default:
        NOT_REACHED();
    }

    log_safe("signal %d (%s) received%s", signo, sig->signame, actionstr);

    if (action != NULL) {
        action();
    }

    if (done) {
        nc_post_run(master_nci);
        exit(1);
    }
}
