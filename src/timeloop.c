/*
 * Copyright (c) 2010-2011 Kevin M. Bowling, <kevin.bowling@kev009.com>, USA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>

#include "craftd-config.h"
#include "craftd.h"
#include "timeloop.h"
#include "network/packets.h"
#include "network/network.h"

/**
 * Internal method to send the silly TCP keepalive packet.  Used only by the
 * timeloop to send at slow interval.
 */
static void
send_keepalive_cb(evutil_socket_t fd, short event, void *arg)
{
    struct PL_entry *player;
    const int8_t keepalivepkt = PID_KEEPALIVE;

    pthread_rwlock_rdlock(&PL_rwlock);

    /* Check if there are players logged in */
    if (SLIST_EMPTY(&PL_head))
    {
        pthread_rwlock_unlock(&PL_rwlock);
        return;
    }

    LOG(LOG_DEBUG, "Sending keepalives");

    SLIST_FOREACH(player, &PL_head, PL_entries)
    {
        struct evbuffer *output = bufferevent_get_output(player->bev);
        evbuffer_add(output, &keepalivepkt, sizeof(keepalivepkt));
    }

    pthread_rwlock_unlock(&PL_rwlock);
    return;
}

/**
 * Internal method to send time update packet. Used only by the
 * timeloop to send at custom interval. Time interval is defined by
 * timepktinterval constant.
 */
static void
send_timeupdate_cb(evutil_socket_t fd, short event, void *arg)
{
    struct PL_entry *player;
    const int8_t timeupdatepkt = PID_TIMEUPDATE;

    pthread_rwlock_rdlock(&PL_rwlock);

    /* Check if there are players logged in */
    if (SLIST_EMPTY(&PL_head))
    {
        pthread_rwlock_unlock(&PL_rwlock);
        return;
    }

    LOG(LOG_DEBUG, "Sending time update");

    SLIST_FOREACH(player, &PL_head, PL_entries)
    {
        send_timeupdate(player, time);
    }

    pthread_rwlock_unlock(&PL_rwlock);
    return;
}

/**
 * Internal method to increase raw time every second. Used only by
 * by the timeloop.
 * 
 * @remarks Note: diffrent rates for Day/Nigt/Sunset/Sunrise can be used.
 * 
 */
static void
timeincrease_cb()
{
//todo: thread lock unlock ?!
    if (time >= 0 & time <= 11999)
    {
        time += dayrate;
        return;
    }
    else if (time >= 12000 & time <= 13799)
    {
        time += sunsetrate;
        return;
    }
    else if (time >= 13800 & time <= 22199)
    {
        time += nightrate;
        return;
    }
    else if (time >= 22200 & time <= 23999)
    {
        time += sunriserate;

        if (time >= 24000)
        {
            time -= 24000;
        }
        return;
    }
    else if (time >= 24000)
    {
        time -= 24000;
    }
    return;
}

/* Public */
void *run_timeloop(void *arg)
{
    struct event_base* tlbase;

    tlbase = event_base_new();
    if (!tlbase)
    {
        LOG(LOG_CRIT, "Time loop event base cannot start");
        exit(1);
    }

    /* Register the keepalive handler */
    struct event *keepalive_event;
    struct timeval keepalive_interval = {10, 0}; // 10 Second Interval

    keepalive_event = event_new(tlbase, -1, EV_PERSIST, send_keepalive_cb, NULL);
    evtimer_add(keepalive_event, &keepalive_interval);

    /* Register the time packet update handler */
    event *timeupdate_event;
    timeval timeupdate_interval = {timepktinterval,0}; // interval defined by constant timepktinterval

    timeupdate_event = event_new(tlbase, -1, EV_PERSIST, send_timeupdate_cb, NULL);
    evtimer_add(timeupdate_event, &timeupdate_interval);
    
     /* Register the time update handler */
    event *timeincrease_event;
    timeval timeincrease_interval = {1,0};

    timeincrease_event = event_new(tlbase, -1, EV_PERSIST, timeincrease_cb, NULL);
    evtimer_add(timeincrease_event, &timeincrease_interval);

    /* Initialize the time loop tail queue.  Work items are added on to the end
     * and a void pointer provides access to heap data
     */
    pthread_rwlock_init(&TLQ_rwlock, NULL);
    TAILQ_INIT(&TLQ_head);
    TLQ_count = 0;

    //TODO will need various timer events w/ different reoccuring time intervals

    LOG(LOG_INFO, "Time event loop started!");

    event_base_dispatch(tlbase);

    return NULL;
}
