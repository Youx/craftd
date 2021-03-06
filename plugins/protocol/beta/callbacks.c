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

#include <zlib.h>

#include <craftd/Logger.h>

#include <beta/World.h>
#include <beta/Region.h>
#include <beta/Player.h>

static
bool
cdbeta_SendChunk (CDServer* server, CDPlayer* player, MCChunkPosition* coord)
{
    DO {
        CDPacketPreChunk pkt = {
            .response = {
                .position = *coord,
                .mode     = true
            }
        };

        CDPacket response = { CDResponse, CDPreChunk, (CDPointer) &pkt };

        CD_PlayerSendPacketAndCleanData(player, &response);
    }

    DO {
        SDEBUG(server, "sending chunk (%d, %d)", coord->x, coord->z);

        MCChunk* chunk = CD_malloc(sizeof(MCChunk));
        CDError  status;

        CD_EventDispatchWithError(status, server, "World.chunk", player->world, coord->x, coord->z, chunk);

        if (status != CDOk) {
            CD_free(chunk);

            return false;
        }

        uLongf written = compressBound(81920);
        Bytef* buffer  = CD_malloc(written);
        Bytef* data    = CD_malloc(81920);
            
        MC_ChunkToByteArray(chunk, data);

        if (compress(buffer, &written, (Bytef*) data, 81920) != Z_OK) {
            SERR(server, "zlib compress failure");

            CD_free(chunk);
            CD_free(data);

            return false;
        }

        SDEBUG(server, "compressed to %ld bytes", written);

        CD_free(chunk);
        CD_free(data);

        CDPacketMapChunk pkt = {
            .response = {
                .position = MC_ChunkPositionToBlockPosition(*coord),

                .size = {
                    .x = 16,
                    .y = 128,
                    .z = 16
                },

                .length = written,
                .item   = (MCByte*) buffer
            }
        };

        CDPacket response = { CDResponse, CDMapChunk, (CDPointer) &pkt };

        CD_PlayerSendPacketAndCleanData(player, &response);
    }

    return true;
}

static
void
cdbeta_ChunkRadiusUnload (CDSet* self, MCChunkPosition* coord, CDPlayer* player)
{
    assert(self);
    assert(coord);
    assert(player);

    DO {
        CDPacketPreChunk pkt = {
            .response = {
                .position = *coord,
                .mode = false
            }
        };

        CDPacket response = { CDResponse, CDPreChunk, (CDPointer) &pkt };

        CD_PlayerSendPacketAndCleanData(player, &response);
    }

    CD_free(coord);
}

static
void
cdbeta_ChunkMemberFree (CDSet* self, MCChunkPosition* coord, void* _)
{
    assert(self);
    assert(coord);

    CD_free(coord);
}

static
void
cdbeta_ChunkRadiusLoad (CDSet* self, MCChunkPosition* coord, CDPlayer* player)
{
    assert(self);
    assert(coord);
    assert(player);

    cdbeta_SendChunk(player->client->server, player, coord);
}

static
void
cdbeta_SendChunkRadius (CDPlayer* player, MCChunkPosition* area, int radius)
{
    CDSet* loadedChunks = (CDSet*) CD_DynamicGet(player, "Player.loadedChunks");
    CDSet* oldChunks    = loadedChunks;
    CDSet* newChunks    = CD_CreateSetWith(400, (CDSetCompare) MC_CompareChunkPosition, (CDSetHash) MC_HashChunkPosition);

    for (int x = -radius; x < radius; x++) {
        for (int z = -radius; z < radius; z++) {
            if ((x * x + z * z) <= (radius * radius)) {
                MCChunkPosition* coord = CD_malloc(sizeof(MCChunkPosition));
                coord->x          = x + area->x;
                coord->z          = z + area->z;

                CD_SetPut(newChunks, (CDPointer) coord);
            }
        }
    }

    CDSet* toRemove = CD_SetMinus(oldChunks, newChunks);
    CDSet* toAdd    = CD_SetMinus(newChunks, oldChunks);

    CD_SetMap(toRemove, (CDSetApply) cdbeta_ChunkRadiusUnload, (CDPointer) player);
    CD_SetMap(toAdd, (CDSetApply) cdbeta_ChunkRadiusLoad, (CDPointer) player);

    CD_DestroySet(toRemove);
    CD_DestroySet(toAdd);
    CD_DestroySet(oldChunks);

    CD_DynamicPut(player, "Player.loadedChunks", (CDPointer) newChunks);
}

static
bool
cdbeta_CoordInRadius(MCChunkPosition *coord, MCChunkPosition *centerCoord, int radius)
{
    if (coord->x >= centerCoord->x - radius &&
        coord->x <= centerCoord->x + radius &&
        coord->z >= centerCoord->z - radius &&
        coord->z <= centerCoord->z + radius)
    {
        return true;
    }

    return false;
}

static
bool
cdbeta_DistanceGreater (MCPrecisePosition a, MCPrecisePosition b, int maxDistance)
{
    return (abs( a.x - b.x ) > maxDistance ||
            abs( a.y - b.y ) > maxDistance ||
            abs( a.z - b.z ) > maxDistance);
}

static
MCRelativePosition
cdbeta_RelativeMove (MCPrecisePosition* a, MCPrecisePosition* b)
{
    MCAbsolutePosition absoluteA = MC_PrecisePositionToAbsolutePosition(*a);
    MCAbsolutePosition absoluteB = MC_PrecisePositionToAbsolutePosition(*b);

    return (MCRelativePosition) {
        .x = absoluteA.x - absoluteB.x,
        .y = absoluteA.y - absoluteB.y,
        .z = absoluteA.z - absoluteB.z
    };
}

static
void
cdbeta_SendPacketToAllInRegion(CDPlayer *player, CDPacket *pkt)
{
  CDList *seenPlayers = (CDList *) CD_DynamicGet(player, "Player.seenPlayers");

  CD_LIST_FOREACH(seenPlayers, it)
  {
    if ( player != (CDPlayer *) CD_ListIteratorValue(it) )
      CD_PlayerSendPacket( (CDPlayer *) CD_ListIteratorValue(it), pkt );
    else
      CERR("We have a player with himself in the List????");
  }
}

static
void
cdbeta_SendUpdatePos(CDPlayer* player, MCPrecisePosition* newPosition, bool andLook, MCFloat pitch, MCFloat yaw)
{
    if (cdbeta_DistanceGreater(player->entity.position, *newPosition, 2) || true) {
        DO {
            CDPacketEntityTeleport pkt;

            pkt.response.entity   = player->entity;
            pkt.response.position = MC_PrecisePositionToAbsolutePosition(*newPosition);

            if (andLook) {
                pkt.response.pitch = pitch;
                pkt.response.rotation = yaw;
            }
            else {
                pkt.response.pitch = player->pitch;
                pkt.response.rotation = player->yaw;
            }
            
            CDPacket response = { CDResponse, CDEntityTeleport, (CDPointer)  &pkt };
            
            cdbeta_SendPacketToAllInRegion(player, &response);
        }
    }
    else {
        if (andLook) {
            CDPacketEntityLookMove pkt = {
                .response = {
                    .entity   = player->entity,
                    .position = CD_RelativeMove(&player->entity.position, newPosition),
                    .yaw      = yaw,
                    .pitch    = pitch
                }
            };


            CDPacket response = { CDResponse, CDEntityLookMove, (CDPointer) &pkt };

            CD_RegionBroadcastPacket(player, &response);
        }
        else {
            CDPacketEntityRelativeMove pkt = {
                .response = {
                    .entity   = player->entity,
                    .position = CD_RelativeMove(&player->entity.position, newPosition)
                }
            };

            CDPacket response = { CDResponse, CDEntityRelativeMove, (CDPointer) &pkt };

            CD_RegionBroadcastPacket(player, &response);
        }
    }
}

static
void
cdbeta_SendNamedPlayerSpawn(CDPlayer *player, CDPlayer *other)
{
    DO {
        CDPacketNamedEntitySpawn pkt = {
            .response = {
                .entity   = other->entity,
                .name     = other->username,
                .pitch    = other->pitch,
                .rotation = other->yaw,

                .item = {
                    .id = 0
                },

                .position = MC_PrecisePositionToAbsolutePosition(other->entity.position)
            }
        };

        CDPacket response = { CDResponse, CDNamedEntitySpawn, (CDPointer) &pkt };

        CD_PlayerSendPacket(player, &response);
    }

    for (int i = 0; i < 5; i++) {
        DO {
            CDPacketEntityEquipment pkt = {
                .response = {
                    .entity = other->entity,

                    .slot   = i,
                    .item   = -1,
                    .damage = 0
                }
            };

            CDPacket response = { CDResponse, CDEntityEquipment, (CDPointer) &pkt };

            CD_PlayerSendPacket(player, &response);
        }
    }

    DO {
        CDPacketEntityTeleport pkt = {
            .response = {
                .entity   = other->entity,
                .pitch    = other->pitch,
                .rotation = 0,
                .position = MC_PrecisePositionToAbsolutePosition(other->entity.position)
            }
        };

        CDPacket response = { CDResponse, CDEntityTeleport, (CDPointer) &pkt };

        CD_PlayerSendPacket(player, &response);
    }

    CD_PlayerSendMessage(player, CD_CreateStringFromFormat("You should now see %s", CD_StringContent(other->username)));
}

static
void
cdbeta_SendDestroyEntity (CDPlayer* player, MCEntity* entity)
{
    DO {
        CDPacketEntityDestroy pkt = {
            .response = {
                .entity = *entity
            }
        };

        CDPacket response = { CDResponse, CDEntityDestroy, (CDPointer) &pkt };

        CD_PlayerSendPacket(player, &response);
    }
}

static
void
cdbeta_CheckPlayersInRegion (CDServer* server, CDPlayer* player, MCChunkPosition *coord, int radius)
{
    CDList* seenPlayers = (CDList*) CD_DynamicGet(player, "Player.seenPlayers");;

    CD_HASH_FOREACH(player->world->players, it) {
        CDPlayer* otherPlayer = (CDPlayer *) CD_HashIteratorValue(it);

        // If we are the player to check just skip
        if (otherPlayer == player) {
            continue;
        }

        MCChunkPosition chunkPos = MC_PrecisePositionToChunkPosition(otherPlayer->entity.position);

        if (cdbeta_CoordInRadius(&chunkPos, coord, radius)) {
            /* If the player is in range, but not in the list. */
            if (!CD_ListContains(seenPlayers, (CDPointer) otherPlayer)) {
                CD_ListPush(seenPlayers, (CDPointer) otherPlayer);
                cdbeta_SendNamedPlayerSpawn(player, otherPlayer);

                CDList *otherSeenPlayers = (CDList *) CD_DynamicGet(otherPlayer, "Player.seenPlayers");
                CD_ListPush(otherSeenPlayers, (CDPointer) player);
                cdbeta_SendNamedPlayerSpawn(otherPlayer, player);
            }
        }
        else {
            /* If the player is out of range but in the list */
            if (CD_ListContains(seenPlayers, (CDPointer) otherPlayer)) {
                CDList *otherSeenPlayers = (CDList *) CD_DynamicGet(otherPlayer, "Player.seenPlayers");

                CD_ListDeleteAll(seenPlayers, (CDPointer) otherPlayer);
                CD_ListDeleteAll(otherSeenPlayers, (CDPointer) player);

                /* Should send both players an update. */
                cdbeta_SendDestroyEntity(player, &otherPlayer->entity);
                cdbeta_SendDestroyEntity(otherPlayer, &player->entity);
            }
        }
    }
}

static
bool
cdbeta_ClientProcess (CDServer* server, CDClient* client, CDPacket* packet)
{
    CDWorld*  world;
    CDPlayer* player = (CDPlayer*) CD_DynamicGet(client, "Client.player");

    if (player && player->world) {
        world = player->world;
    }
    else {
        world = (CDWorld*) CD_DynamicGet(server, "World.default");
    }

    switch (packet->type) {
        case CDKeepAlive: {
            SDEBUG(server, "%s is still alive", player ? CD_StringContent(player->username) : client->ip);
        } break;

        case CDLogin: {
            CDPacketLogin* data = (CDPacketLogin*) packet->data;

            pthread_mutex_lock(&_lock.login);

            SLOG(server, LOG_NOTICE, "%s tried login with client version %d", CD_StringContent(data->request.username), data->request.version);

            if (data->request.version != CRAFTD_PROTOCOL_VERSION) {
                CD_ServerKick(server, client, CD_CreateStringFromFormat(
                    "Protocol mismatch, we support %d, you're using %d.",
                    CRAFTD_PROTOCOL_VERSION, data->request.version));

                return false;
            }

            if (CD_HashHasKey(world->players, CD_StringContent(data->request.username))) {
                SLOG(server, LOG_NOTICE, "%s: nick exists on the server", CD_StringContent(data->request.username));

                if (server->config->cache.game.standard) {
                    CD_ServerKick(server, client, CD_CreateStringFromFormat("%s nick already exists",
                        CD_StringContent(data->request.username)));

                    CD_EventDispatch(server, "Player.login", player, false);

                    return false;
                }
                else {
                    player->username = CD_CreateStringFromFormat("Player_%d", player->entity.id);
                }
            }
            else {
                player->username = CD_CloneString(data->request.username);
            }

            player->world = world;

            CD_HashPut(world->players, CD_StringContent(player->username), (CDPointer) player);
            CD_MapPut(world->entities, player->entity.id, (CDPointer) player);

            pthread_mutex_unlock(&_lock.login);

            DO {
                CDPacketLogin pkt = {
                    .response = {
                        .id         = player->entity.id,
                        .serverName = CD_CreateStringFromCString(""),
                        .motd       = CD_CreateStringFromCString(""),
                        .mapSeed    = 0,
                        .dimension  = 0
                    }
                };

                CDPacket response = { CDResponse, CDLogin, (CDPointer) &pkt };

                CD_PlayerSendPacketAndCleanData(player, &response);
            }

            MCChunkPosition spawnChunk = MC_BlockPositionToChunkPosition(world->spawnPosition);

            // Hack in a square send for login
            for (int i = -7; i < 8; i++) {
                for ( int j = -7; j < 8; j++) {
                    MCChunkPosition coords = {
                        .x = spawnChunk.x + i,
                        .z = spawnChunk.z + j
                    };

                    if (!cdbeta_SendChunk(server, player, &coords)) {
                        return false;
                    }
                }
            }

            /* Send Spawn Position to initialize compass */
            DO {
                CDPacketSpawnPosition pkt = {
                    .response = {
                        .position = world->spawnPosition
                    }
                };

                CDPacket response = { CDResponse, CDSpawnPosition, (CDPointer) &pkt };

                CD_PlayerSendPacketAndCleanData(player, &response);
            }

            DO {
                MCPrecisePosition pos = MC_BlockPositionToPrecisePosition(world->spawnPosition);
                CDPacketPlayerMoveLook pkt = {
                    .response = {
                        .position = {
                            .x = pos.x,
                            .y = pos.y + 6,
                            .z = pos.z
                        },

                        .stance = pos.y + 6.1,
                        .yaw    = 0,
                        .pitch  = 0,

                        .is = {
                            .onGround = false
                        }
                    }
                };

                CDPacket response = { CDResponse, CDPlayerMoveLook, (CDPointer) &pkt };

                CD_PlayerSendPacketAndCleanData(player, &response);
            }

            CD_EventDispatch(server, "Player.login", player, true);
        } break;

        case CDHandshake: {
            CDPacketHandshake* data = (CDPacketHandshake*) packet->data;

            SLOG(server, LOG_NOTICE, "%s tried handshake", CD_StringContent(data->request.username));

            CDPacketHandshake pkt = {
                .response = {
                    .hash = CD_CreateStringFromCString("-")
                }
            };

            player = CD_CreatePlayer(client);

            CD_DynamicPut(client, "Client.player", (CDPointer) player);

            CDPacket response = { CDResponse, CDHandshake, (CDPointer) &pkt };

            CD_PlayerSendPacketAndCleanData(player, &response);
        } break;

        case CDChat: {
            CDPacketChat* data = (CDPacketChat*) packet->data;

            if (CD_StringEmpty(player->username)) {
                break;
            }

            if (CD_StringStartWith(data->request.message, _config.commandChar)) {
                CDString* commandString = CD_CreateStringFromOffset(data->request.message, 1, 0);
                CD_EventDispatch(server, "Player.command", player, commandString);
                CD_DestroyString(commandString);
            }
            else {
                CD_EventDispatch(server, "Player.chat", player, data->request.message);
            }
        } break;

        case CDOnGround: {
            // Stub.  Probably not needed
        } break;

        case CDPlayerPosition: {
            // Stub.  Do dead reckoning or some other sanity check for data
            // and send CD_SetDifference of chunks on boundary change.

            CDPacketPlayerPosition* data = (CDPacketPlayerPosition*) packet->data;

            MCChunkPosition newChunk = MC_PrecisePositionToChunkPosition(data->request.position);
            MCChunkPosition curChunk = MC_PrecisePositionToChunkPosition(player->entity.position);

            if (!MC_ChunkPositionEqual(newChunk, curChunk)) {
                cdbeta_SendChunkRadius(player, &newChunk, 10);

                cdbeta_CheckPlayersInRegion(server, player, &newChunk, 5);
            }

            cdbeta_SendUpdatePos(player, &data->request.position, false, 0, 0);

            player->entity.position = data->request.position;
        } break;

        case CDPlayerLook: {
            // Stub.  Add input validation and sanity checks.

            CDPacketPlayerLook* data = (CDPacketPlayerLook*) packet->data;

            player->yaw   = data->request.yaw;
            player->pitch = data->request.pitch;

            DO {
              CDPacketEntityLook pkt;

              pkt.response.entity = player->entity;
              pkt.response.pitch = player->pitch;
              pkt.response.yaw = player->yaw;

              CDPacket response = { CDResponse, CDEntityLook, (CDPointer) &pkt };

              cdbeta_SendPacketToAllInRegion(player, &response);
            }
        } break;

        case CDPlayerMoveLook: {
            // Stub.  Do dead reckoning or some other sanity check for data
            // and send CD_SetDifference of chunks on boundary change.

            CDPacketPlayerMoveLook* data = (CDPacketPlayerMoveLook*) packet->data;

            MCChunkPosition oldChunk = MC_PrecisePositionToChunkPosition(player->entity.position);
            MCChunkPosition newChunk = MC_PrecisePositionToChunkPosition(data->request.position);

            if (!MC_ChunkPositionEqual(oldChunk, newChunk)) {
                cdbeta_SendChunkRadius(player, &newChunk, 10);

                cdbeta_CheckPlayersInRegion(server, player, &newChunk, 5);
            }

            cdbeta_SendUpdatePos(player, &data->request.position, true, data->request.pitch, data->request.yaw);

            player->entity.position = data->request.position;
            player->yaw             = data->request.yaw;
            player->pitch           = data->request.pitch;
        } break;

        case CDDisconnect: {
            CDPacketDisconnect* data = (CDPacketDisconnect*) packet->data;

            CD_ServerKick(server, client, CD_CloneString(data->request.reason));
        } break;

        default: {
            if (player) {
                SERR(server, "unimplemented packet 0x%.2X from %s (%s)", packet->type, 
                    player->client->ip, CD_StringContent(player->username));
            }
            else {
                SERR(server, "unimplemented packet 0x%.2X from %s", packet->type, player->client->ip);
            }
        }
    }

    return true;
}

static
bool
cdbeta_ClientProcessed (CDServer* server, CDClient* client, CDPacket* packet)
{
    return true;
}

static
bool
cdbeta_ClientConnect (CDServer* server, CDClient* client)
{
    return true;
}

static
bool
cdbeta_PlayerLogin (CDServer* server, CDPlayer* player, int status)
{
    if (!status) {
        return true;
    }

    DO {
        CDPacketTimeUpdate pkt = {
            .response = {
                .time = CD_WorldGetTime(player->world)
            }
        };

        CDPacket packet = { CDResponse, CDTimeUpdate, (CDPointer) &pkt };

        CD_PlayerSendPacket(player, &packet);
    }

    CD_WorldBroadcastMessage(player->world, MC_StringColor(CD_CreateStringFromFormat("%s has joined the game",
                CD_StringContent(player->username)), MCColorYellow));


    CD_DynamicPut(player, "Player.loadedChunks", (CDPointer) CD_CreateSetWith(
        400, (CDSetCompare) MC_CompareChunkPosition, (CDSetHash) MC_HashChunkPosition));

    CD_DynamicPut(player, "Player.seenPlayers", (CDPointer) CD_CreateList());

    MCChunkPosition playerChunk = MC_PrecisePositionToChunkPosition(player->entity.position);

    cdbeta_CheckPlayersInRegion(server, player, &playerChunk, 5);

    return true;
}

static
bool
cdbeta_PlayerLogout (CDServer* server, CDPlayer* player)
{
    CD_WorldBroadcastMessage(player->world, MC_StringColor(CD_CreateStringFromFormat("%s has left the game",
        CD_StringContent(player->username)), MCColorYellow));

    CDList* seenPlayers = (CDList*) CD_DynamicDelete(player, "Player.seenPlayers");

    CD_LIST_FOREACH(seenPlayers, it) {
        CDPlayer* other            = (CDPlayer*) CD_ListIteratorValue(it);
        CDList*   otherSeenPlayers = (CDList*) CD_DynamicGet(other, "Player.seenPlayers");

        cdbeta_SendDestroyEntity(other, &player->entity);
        CD_ListDeleteAll(otherSeenPlayers, (CDPointer) player);
    }

    CD_HashDelete(player->world->players, CD_StringContent(player->username));
    CD_MapDelete(player->world->entities, player->entity.id);

    CD_DestroyList(seenPlayers);

    CDSet* chunks = (CDSet*) CD_DynamicDelete(player, "Player.loadedChunks");

    if (chunks) {
        CD_SetMap(chunks, (CDSetApply) cdbeta_ChunkMemberFree, CDNull);
        CD_DestroySet(chunks);
    }

    return true;
}

static
bool
cdbeta_ClientDisconnect (CDServer* server, CDClient* client, bool status)
{
    CDPlayer* player = (CDPlayer*) CD_DynamicGet(client, "Client.player");

    if (player->world) {
        CD_EventDispatch(server, "Player.logout", player, status);
    }

    return true;
}

static
bool
cdbeta_ClientKick (CDServer* server, CDClient* client, CDString* reason)
{
    DO {
        CDPacketDisconnect pkt = {
            .response = {
                .reason = reason
            }
        };

        CDPacket  packet = { CDResponse, CDDisconnect, (CDPointer) &pkt };
        CDBuffer* buffer = CD_PacketToBuffer(&packet);

        CD_ClientSendBuffer(client, buffer);

        CD_DestroyBuffer(buffer);
    }

    return true;
}

static
bool
cdbeta_PlayerCommand (CDServer* server, CDPlayer* player, CDString* command)
{
    CDRegexpMatches* matches = CD_RegexpMatchString("^(\\w+)(?:\\s+(.*?))?$", CDRegexpNone, command);

    if (matches) {
        CD_PlayerSendMessage(player, MC_StringColor(CD_CreateStringFromFormat("%s: unknown command",
            CD_StringContent(matches->item[1])), MCColorRed));
    }

    return false;
}

static
bool
cdbeta_PlayerChat (CDServer* server, CDPlayer* player, CDString* message)
{
    SLOG(server, LOG_NOTICE, "<%s> %s", CD_StringContent(player->username),
            CD_StringContent(message));

    CD_ServerBroadcast(server, CD_CreateStringFromFormat("<%s> %s",
        CD_StringContent(player->username),
        CD_StringContent(message)));

    return true;
}

static
bool
cdbeta_PlayerDestroy (CDServer* server, CDPlayer* player)
{
    return true;
}
