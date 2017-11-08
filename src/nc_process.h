#include <nc_core.h>

#ifndef _NC_PROCESS_H
#define _NC_PROCESS_H

extern bool pm_reload;
extern bool pm_respawn;

rstatus_t nc_multi_processes_cycle(struct instance *parent_nci);
rstatus_t nc_spawn_workers(int n, struct instance *parent_nci);
void      nc_worker_process(int worker_id, struct instance *nci);
rstatus_t nc_single_process_cycle(struct instance *nci);

#endif //_NC_PROCESS_H
