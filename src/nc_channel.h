#ifndef _NC_CHANNEL_H_
#define _NC_CHANNEL_H_

struct channel {
    int fds[2];
};

struct chan_msg {
    int command;
};

struct channel *nc_alloc_channel(void);
void nc_dealloc_channel(struct channel *ch);
void nc_close_channel(struct channel *ch);
int nc_read_channel(int fd, struct chan_msg *chmsg);
int nc_write_channel(int fd, struct chan_msg *chmsg);
int nc_add_channel_event(struct event_base  *evb, int fd);
#endif
