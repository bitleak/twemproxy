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

#include <signal.h>
#include <nc_core.h>

#ifdef NC_HAVE_EPOLL

#include <sys/epoll.h>

struct event_base *
event_base_create(int nevent, event_cb_t cb)
{
    struct event_base *evb;
    int status, ep;
    struct epoll_event *event;
    struct ev_data *evd;

    ASSERT(nevent > 0);

    ep = epoll_create(nevent);
    if (ep < 0) {
        log_error("epoll create of size %d failed: %s", nevent, strerror(errno));
        return NULL;
    }

    event = nc_calloc(nevent, sizeof(*event));
    if (event == NULL) {
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }

    evd = nc_calloc(nevent, sizeof(*evd));
    if (evd == NULL) {
        status = close(ep);
        nc_free(event);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }

    evb = nc_alloc(sizeof(*evb));
    if (evb == NULL) {
        nc_free(event);
        nc_free(evd);
        status = close(ep);
        if (status < 0) {
            log_error("close e %d failed, ignored: %s", ep, strerror(errno));
        }
        return NULL;
    }

    evb->ep = ep;
    evb->evd = evd;
    evb->nevd = nevent;
    evb->event = event;
    evb->nevent = nevent;
    evb->cb = cb;

    log_debug(LOG_INFO, "e %d with nevent %d", evb->ep, evb->nevent);

    return evb;
}

void
event_base_destroy(struct event_base *evb)
{
    int status;

    if (evb == NULL) {
        return;
    }

    ASSERT(evb->ep > 0);

    nc_free(evb->event);
    nc_free(evb->evd);

    status = close(evb->ep);
    if (status < 0) {
        log_error("close e %d failed, ignored: %s", evb->ep, strerror(errno));
    }
    evb->ep = -1;

    nc_free(evb);
}

static void
event_base_need_resize(struct event_base *evb, int fd)
{
    int new_size;
    struct ev_data *new_evd, *old_evd;

    if (fd < evb->nevd) {
        return;
    }
    new_size = fd >= evb->nevd*2 ? fd + 1 : evb->nevd*2;
    new_evd = nc_calloc(new_size, sizeof(struct ev_data));
    if (new_evd != NULL) {
        old_evd = evb->evd;
        memcpy(new_evd, old_evd, evb->nevd*sizeof(struct ev_data));
        evb->evd = new_evd;
        evb->nevd = new_size;
        nc_free(old_evd);
    }
}

int
event_add(struct event_base *evb, int fd, int mask, event_cb_t cb, void *priv)
{
    int op;
    struct epoll_event event = {0};
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(cb != NULL);
    ASSERT(fd > 0);

    event_base_need_resize(evb, fd);
    if (fd >= evb->nevd) {
        return -1;
    }
    op = evb->evd[fd].mask == EVENT_NONE ? EPOLL_CTL_ADD : EPOLL_CTL_MOD;

    evb->evd[fd].cb = cb;
    evb->evd[fd].priv = priv;
    evb->evd[fd].mask |= mask;
    if (mask & EVENT_READ) {
        event.events |= (uint32_t)(EPOLLIN | EPOLLET);
    }
    if (mask & EVENT_WRITE) {
        event.events |= (uint32_t)(EPOLLIN | EPOLLOUT | EPOLLET);
    }
    event.data.fd = fd;
    return epoll_ctl(ep, op, fd, &event);
}

int
event_del(struct event_base *evb, int fd, int delmask)
{
    int mask;
    struct epoll_event event = {0};
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(fd > 0);

    if (fd >= evb->nevd) {
        return -1;
    }
    mask = evb->evd[fd].mask & (~delmask);
    event.events = 0;
    event.data.fd = fd;

    evb->evd[fd].mask = EVENT_NONE;
    if (mask & EVENT_READ) {
        evb->evd[fd].mask |= EVENT_READ;
        event.events = (uint32_t)(EPOLLIN | EPOLLET);
    }
    if (mask & EVENT_WRITE) {
        evb->evd[fd].mask |= EVENT_WRITE;
        event.events |= (uint32_t)(EPOLLOUT | EPOLLET);
    }
    if (mask == EVENT_NONE) {
        evb->evd[fd].cb = NULL;
        evb->evd[fd].priv = NULL;
        /* Note, Kernel < 2.6.9 requires a non null event pointer even for
         *  EPOLL_CTL_DEL. */
        return epoll_ctl(ep, EPOLL_CTL_DEL, fd, &event);
    }
    return epoll_ctl(ep, EPOLL_CTL_MOD, fd, &event);
}

int
event_add_in(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    if (c->recv_active) {
        return 0;
    }
    status = event_add(evb, c->sd, EVENT_READ, evb->cb, c);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->recv_active = 1;
    }

    return status;
}

int
event_del_in(struct event_base *evb, struct conn *c)
{
    return 0;
}

int
event_add_out(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (c->send_active) {
        return 0;
    }

    status = event_add(evb, c->sd, EVENT_WRITE, evb->cb, c);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
    }

    return status;
}

int
event_del_out(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);
    ASSERT(c->recv_active);

    if (!c->send_active) {
        return 0;
    }

    status = event_del(evb, c->sd, EVENT_WRITE);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 0;
    }

    return status;
}

int
event_add_conn(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    status = event_add(evb, c->sd, EVENT_READ|EVENT_WRITE, evb->cb, c);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->send_active = 1;
        c->recv_active = 1;
    }

    return status;
}

int
event_del_conn(struct event_base *evb, struct conn *c)
{
    int status;
    int ep = evb->ep;

    ASSERT(ep > 0);
    ASSERT(c != NULL);
    ASSERT(c->sd > 0);

    status = event_del(evb, c->sd, EVENT_READ|EVENT_WRITE);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, c->sd,
                  strerror(errno));
    } else {
        c->recv_active = 0;
        c->send_active = 0;
    }

    return status;
}

int
event_wait(struct event_base *evb, int timeout)
{
    int ep = evb->ep;
    struct epoll_event *event = evb->event;
    int nevent = evb->nevent;
    sigset_t set;

    ASSERT(ep > 0);
    ASSERT(event != NULL);
    ASSERT(nevent > 0);

    sigemptyset(&set);
    sigaddset(&set, SIGALRM);
    for (;;) {
        int i, nsd;

        // use epoll_pwait instead of epoll_wait,
        // allow to interupt epoll loop when the worker shutdown timeout was reached
        nsd = epoll_pwait(ep, event, nevent, timeout, &set);
        if (nsd > 0) {
            for (i = 0; i < nsd; i++) {
                struct epoll_event *ev = &evb->event[i];
                struct ev_data *evd = &evb->evd[ev->data.fd];
                uint32_t events = 0;

                log_debug(LOG_VVERB, "epoll %04"PRIX32" triggered on conn %p",
                          ev->events, ev->data.ptr);

                if (ev->events & EPOLLERR) {
                    events |= EVENT_ERR;
                }

                if (ev->events & (EPOLLIN | EPOLLHUP)) {
                    events |= EVENT_READ;
                }

                if (ev->events & EPOLLOUT) {
                    events |= EVENT_WRITE;
                }

                if (evd->cb != NULL) {
                    evd->cb(evb, evd->priv, events);
                }
            }
            return nsd;
        }

        if (nsd == 0) {
            if (timeout == -1) {
               log_error("epoll wait on e %d with %d events and %d timeout "
                         "returned no events", ep, nevent, timeout);
                return -1;
            }

            return 0;
        }

        if (errno == EINTR) {
            continue;
        }

        log_error("epoll wait on e %d with %d events failed: %s", ep, nevent,
                  strerror(errno));
        return -1;
    }

    NOT_REACHED();
}

void
event_loop_stats(event_stats_cb_t cb, void *arg)
{
    struct stats *st = arg;
    int status, ep;
    struct epoll_event ev;

    ep = epoll_create(1);
    if (ep < 0) {
        log_error("epoll create failed: %s", strerror(errno));
        return;
    }

    ev.data.fd = st->sd;
    ev.events = EPOLLIN;

    status = epoll_ctl(ep, EPOLL_CTL_ADD, st->sd, &ev);
    if (status < 0) {
        log_error("epoll ctl on e %d sd %d failed: %s", ep, st->sd,
                  strerror(errno));
        goto error;
    }

    for (;;) {
        int n;

        n = epoll_wait(ep, &ev, 1, st->interval);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            log_error("epoll wait on e %d with m %d failed: %s", ep,
                      st->sd, strerror(errno));
            break;
        }

        cb(st, &n);
    }

error:
    status = close(ep);
    if (status < 0) {
        log_error("close e %d failed, ignored: %s", ep, strerror(errno));
    }
    ep = -1;
}

#endif /* NC_HAVE_EPOLL */
