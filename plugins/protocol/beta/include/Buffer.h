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

#ifndef CRAFTD_BETA_BUFFER_H
#define CRAFTD_BETA_BUFFER_H

#include <craftd/Buffer.h>

#include <beta/minecraft.h>

/**
 * Add data to a buffer with data from the given format.
 *
 * Format types:
 *     b: MCByte
 *     s: MCShort
 *     i: MCInteger
 *     l: MCLong
 *
 *     f: MCFloat
 *     d: MCDouble
 *
 *     B: MCBoolean
 *     S: MCString
 *     M: MCMetadata
 *
 * @param format The format string
 */
void CD_BufferAddFormat (CDBuffer* self, const char* format, ...);

void CD_BufferAddByte (CDBuffer* self, MCByte data);

void CD_BufferAddShort (CDBuffer* self, MCShort data);

void CD_BufferAddInteger (CDBuffer* self, MCInteger data);

void CD_BufferAddLong (CDBuffer* self, MCLong data);

void CD_BufferAddFloat (CDBuffer* self, MCFloat data);

void CD_BufferAddDouble (CDBuffer* self, MCDouble data);

void CD_BufferAddBoolean (CDBuffer* self, MCBoolean data);

void CD_BufferAddString (CDBuffer* self, MCString data);

void CD_BufferAddMetadata (CDBuffer* self, MCMetadata* data);




/**
 * Remove data from a buffer with data from the given format.
 *
 * Format types:
 *     b: MCByte
 *     s: MCShort
 *     i: MCInteger
 *     l: MCLong
 *
 *     f: MCFloat
 *     d: MCDouble
 *
 *     B: MCBoolean
 *     S: MCString
 *     M: MCMetadata
 *
 * @param format The format string
 */
void CD_BufferRemoveFormat (CDBuffer* self, const char* format, ...);

MCByte CD_BufferRemoveByte (CDBuffer* self);

MCShort CD_BufferRemoveShort (CDBuffer* self);

MCInteger CD_BufferRemoveInteger (CDBuffer* self);

MCLong CD_BufferRemoveLong (CDBuffer* self);

MCFloat CD_BufferRemoveFloat (CDBuffer* self);

MCDouble CD_BufferRemoveDouble (CDBuffer* self);

MCBoolean CD_BufferRemoveBoolean (CDBuffer* self);

MCString CD_BufferRemoveString (CDBuffer* self);

MCMetadata* CD_BufferRemoveMetadata (CDBuffer* self);

#endif
