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

#ifndef CRAFTD_PLAYER_H
#define CRAFTD_PLAYER_H

#include "minecraft.h"
#include "Hash.h"
#include "Packet.h"

struct _CDServer;

typedef enum _CDPlayerStatus {
    CDPlayerIdle,
    CDPlayerInput,
    CDPlayerProcess
} CDPlayerStatus;

typedef struct _CDPlayer {
    MCEntity entity;

    struct _CDServer* server;

    bstring   name;
    char      ip[128];

    evutil_socket_t     socket;
    struct bufferevent* buffer;

    CDHash* _private;

    CDPlayerStatus status;

    bool pending;

    struct {
        pthread_rwlock_t status;
        pthread_rwlock_t pending;
    } lock;
} CDPlayer;

CDPlayer* CD_CreatePlayer (struct _CDServer* server);

void CD_DestroyPlayer (CDPlayer* self);

void CD_SetPlayerName (const char* name);

void CD_PlayerSendPacket (CDPlayer* self, CDPacket* packet);

void CD_PlayerSendRaw (CDPlayer* self, CDString* data);

#endif