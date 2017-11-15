#include <fcntl.h>
#include <nc_core.h>
#include <nc_process.h>

struct channel*
nc_alloc_channel(void)
{
    struct channel *ch;

    ch = nc_alloc(sizeof(*ch));
    if (ch == NULL) {
        log_error("alloc channel failed:", strerror(errno));
        return NULL;
    }
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, ch->fds) == -1) {
        nc_free(ch);
        log_error("socketpair() failed:", strerror(errno));
        return NULL;
    }
    if (nc_set_nonblocking(ch->fds[0]) == -1
        || nc_set_nonblocking(ch->fds[1]) == -1 ) {
        nc_dealloc_channel(ch);
        log_error("socket_set_nonblocking() failed:", strerror(errno));
        return NULL;
    }
    return ch;
}

void
nc_close_channel(struct channel *ch)
{
    close(ch->fds[0]);
    close(ch->fds[1]);
}

void
nc_dealloc_channel(struct channel *ch)
{
    nc_close_channel(ch);
    nc_free(ch);
}

static rstatus_t
channel_recv(void *evb, void *priv)
{
    int fd;
    int n;
    struct chan_msg msg;

    fd = (int) priv;
    for (;;) {
        n = nc_read_channel(fd, &msg);
        if (n == NC_ERROR) {
            event_del(evb, fd, EVENT_READ|EVENT_WRITE);
            return NC_ERROR;
        }
        if (n == NC_EAGAIN) {
            return NC_EAGAIN;
        }
        switch (msg.command) {
            case NC_CMD_TERMINATE:
                pm_terminate = true;
                log_warn("terminate signal was received");
                break;
            case NC_CMD_QUIT:
                pm_quit = true;
                log_warn("quit signal was receviced");
                break;
            case NC_CMD_LOG_REOPEN:
                log_reopen();
                break;
            case NC_CMD_LOG_LEVEL_UP:
                log_level_up();
                break;
            case NC_CMD_LOG_LEVEL_DOWN:
                log_level_down();
                break;
        }
    }
    return NC_OK;
}

static rstatus_t
channel_send(void  *evb, void *priv)
{
    return NC_OK;
}

static rstatus_t
channel_error(void *evb, void *priv)
{
    int fd;

    fd = (int) priv;
    return event_del(evb, fd, EVENT_READ|EVENT_WRITE);

}

static rstatus_t
channel_event_cb(void *evb, void *priv, uint32_t events)
{
    if (events & EVENT_ERR) {
        return channel_error(evb, priv);
    }
    if (events & EVENT_READ) {
        return channel_recv(evb, priv);
    }
    if (events & EVENT_WRITE) {
        return channel_send(evb, priv);
    }
    return NC_OK;
}

int
nc_add_channel_event(struct event_base *evb, int fd)
{
    return event_add(evb, fd, EVENT_WRITE|EVENT_READ, channel_event_cb, (void *)(long)fd);
}

int
nc_write_channel(int fd, struct chan_msg *chmsg)
{
    ssize_t n;
    struct iovec iov[1];
    struct msghdr msg;

    iov[0].iov_base = (char *) chmsg;
    iov[0].iov_len  = sizeof(*chmsg);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    n = sendmsg(fd, &msg, 0);
    if (n == -1) {
        if (errno == EAGAIN) {
            return NC_EAGAIN;
        }
        return NC_ERROR;
    }
    return (int)n;
}

int
nc_read_channel(int fd, struct chan_msg *chmsg)
{
    ssize_t n;
    struct iovec iov[1];
    struct msghdr msg;

    iov[0].iov_base = (char *) chmsg;
    iov[0].iov_len = sizeof(*chmsg);

    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    n = recvmsg(fd, &msg, 0);
    if (n == -1) {
        if (errno == EAGAIN) {
            return NC_EAGAIN;
        }
        return NC_ERROR;
    }
    if (n == 0) {
        return NC_ERROR;
    }
    if (n < (int)sizeof(*chmsg)) {
        return NC_ERROR;
    }
    return (int)n;
}
