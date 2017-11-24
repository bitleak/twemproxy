#include <nc_core.h>

#ifndef _NC_PROCESS_H
#define _NC_PROCESS_H

#define SHARED_MEMORY_SIZE 1048576

#define ROLE_MASTER 1
#define ROLE_WORKER 2

#define NC_CMD_QUIT      1
#define NC_CMD_TERMINATE 2
#define NC_CMD_LOG_REOPEN 3
#define NC_CMD_LOG_LEVEL_UP 4
#define NC_CMD_LOG_LEVEL_DOWN 5

extern bool pm_reload;
extern bool pm_respawn;
extern char pm_myrole;
extern bool pm_quit;
extern struct instance *master_nci;
extern bool pm_terminate;

rstatus_t nc_multi_processes_cycle(struct instance *parent_nci);
rstatus_t nc_single_process_cycle(struct instance *nci);
void      nc_reload_config(void);
void      nc_reap_worker(void);
void      nc_signal_workers(struct array *workers, int command);

#endif //_NC_PROCESS_H
