#include <nc_core.h>

#ifndef TWEMPROXY_NC_PROCESS_H
#define TWEMPROXY_NC_PROCESS_H

extern bool pm_reload;
extern bool pm_respawn;

rstatus_t nc_multi_processes_cycle(struct instance *nci);
rstatus_t nc_spawn_workers(int n, struct instance *nci);
rstatus_t nc_worker_process(int worker_id, struct instance *nci);
rstatus_t nc_single_process_cycle(struct instance *nci);

#endif //TWEMPROXY_NC_PROCESS_H
