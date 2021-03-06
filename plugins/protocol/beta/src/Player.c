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

#include <craftd/Server.h>

#include <beta/Player.h>

CDPlayer*
CD_CreatePlayer (CDClient* client)
{
    CDPlayer* self = CD_malloc(sizeof(CDPlayer));

    assert(self);

    self->client = client;

    self->entity.id         = 0;
    self->entity.type       = MCEntityPlayer;
    self->entity.position.x = 0;
    self->entity.position.y = 0;
    self->entity.position.z = 0;

    self->username = NULL;
    self->world    = NULL;

    DYNAMIC(self) = CD_CreateDynamic();
    ERROR(self)   = CDNull;

    return self;
}

void
CD_DestroyPlayer (CDPlayer* self)
{
    CD_EventDispatch(self->client->server, "Player.destroy", self);

    if (self->username) {
        CD_DestroyString(self->username);
    }

    CD_DestroyDynamic(DYNAMIC(self));

    CD_free(self);
}

void
CD_PlayerSendMessage (CDPlayer* self, CDString* message)
{
    DO {
        CDPacketChat pkt = {
            .response = {
                .message = message
            }
        };

        CDPacket response = { CDResponse, CDChat, (CDPointer) &pkt };

        CD_PlayerSendPacketAndCleanData(self, &response);
    }
}

void
CD_PlayerSendPacket (CDPlayer* self, CDPacket* packet)
{
    if (!self || !self->client || !self->client->buffers) {
        return;
    }

    CDBuffer* data = CD_PacketToBuffer(packet);

    CD_ClientSendBuffer(self->client, data);

    CD_DestroyBuffer(data);
}

void
CD_PlayerSendPacketAndClean (CDPlayer* self, CDPacket* packet)
{
    if (!self || !self->client || !self->client->buffers) {
        return;
    }

    CDBuffer* data = CD_PacketToBuffer(packet);

    CD_ClientSendBuffer(self->client, data);

    CD_DestroyBuffer(data);
    CD_DestroyPacket(packet);
}

void
CD_PlayerSendPacketAndCleanData (CDPlayer* self, CDPacket* packet)
{
    if (!self || !self->client || !self->client->buffers) {
        return;
    }

    CDBuffer* data = CD_PacketToBuffer(packet);

    CD_ClientSendBuffer(self->client, data);

    CD_DestroyBuffer(data);
    CD_DestroyPacketData(packet);
}
