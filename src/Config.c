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

#include <craftd/Config.h>
#include <craftd/Logger.h>

// TODO: Implement real parsing of the json file
CDConfig*
CD_ParseConfig (const char* path)
{
    CDConfig* self = CD_malloc(sizeof(CDConfig));

    self->data = json_load_file(path, 0, &self->error);

    if (!self->data) {
        ERR("error on line %d while parsing %s: %s", self->error.line, path, self->error.text);

        CD_DestroyConfig(self);

        return NULL;
    }

    self->cache.daemonize = true;

    self->cache.connection.port         = 25565;
    self->cache.connection.backlog      = 16;
    self->cache.connection.simultaneous = 3;

    self->cache.connection.bind.ipv4.sin_family      = AF_INET;
    self->cache.connection.bind.ipv4.sin_addr.s_addr = INADDR_ANY;
    self->cache.connection.bind.ipv4.sin_port        = htons(self->cache.connection.port);

    self->cache.connection.bind.ipv6.sin6_family = AF_INET6;
    self->cache.connection.bind.ipv6.sin6_addr   = in6addr_any;
    self->cache.connection.bind.ipv6.sin6_port   = htons(self->cache.connection.port);

    self->cache.httpd.enabled         = true;
    self->cache.httpd.connection.port = 25566;

    self->cache.httpd.connection.bind.ipv4 = "0.0.0.0";
    self->cache.httpd.connection.bind.ipv6 = "::";

    self->cache.files.motd = "/etc/craftd/motd.conf";

    self->cache.workers = 2;

    self->cache.game.players.max = 0;

    J_DO {
        J_IN(server, self->data, "server") {
            J_BOOL(server, "daemonize",   self->cache.daemonize);
            J_INT(server,  "workers",     self->cache.workers);

            J_IN(game, server, "game") {
                J_IN(players, game, "players") {
                    J_INT(players, "max", self->cache.game.players.max);
                }

                J_BOOL(game, "standard", self->cache.game.standard);
            }

            J_IN(files, server, "files") {
                J_STRING(files, "motd",  self->cache.files.motd);
            }

            J_IN(connection, server, "connection") {
                J_INT(connection, "port",         self->cache.connection.port);
                J_INT(connection, "backlog",      self->cache.connection.backlog);
                J_INT(connection, "simultaneous", self->cache.connection.simultaneous);

                self->cache.connection.bind.ipv4.sin_port  = htons(self->cache.connection.port);
                self->cache.connection.bind.ipv6.sin6_port = htons(self->cache.connection.port);

                J_IN(bind, connection, "bind") {
                    J_IF_STRING(bind, "ipv4") {
                        if (evutil_inet_pton(AF_INET, J_STRING_VALUE, &self->cache.connection.bind.ipv4.sin_addr) != 1) {
                            self->cache.connection.bind.ipv4.sin_addr.s_addr = INADDR_ANY;
                        }
                    }

                    J_IF_STRING(bind, "ipv6") {
                        if (evutil_inet_pton(AF_INET6, J_STRING_VALUE, &self->cache.connection.bind.ipv6.sin6_addr) != 1) {
                            self->cache.connection.bind.ipv6.sin6_addr = in6addr_any;
                        }
                    }
                }
            }
        }

        J_IN(httpd, self->data, "httpd") {
            J_BOOL(httpd,   "enabled", self->cache.httpd.enabled);
            J_STRING(httpd, "root",    self->cache.httpd.root);

            J_IN(connection, httpd, "connection") {
                J_INT(connection, "port", self->cache.httpd.connection.port);

                J_IN(bind, connection, "bind") {
                    J_STRING(bind, "ipv4", self->cache.httpd.connection.bind.ipv4);
                    J_STRING(bind, "ipv6", self->cache.httpd.connection.bind.ipv6);
                }
            }
        }
    }

    return self;
}

void
CD_DestroyConfig (CDConfig* self)
{
    if (self->data) {
        json_delete(self->data);
    }

    CD_free(self);
}

