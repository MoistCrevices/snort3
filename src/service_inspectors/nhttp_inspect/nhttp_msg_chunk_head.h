/****************************************************************************
 *
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
 * Copyright (C) 2003-2013 Sourcefire, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License Version 2 as
 * published by the Free Software Foundation.  You may not use, modify or
 * distribute this program under any other version of the GNU General
 * Public License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 ****************************************************************************/

//
//  @author     Tom Peters <thopeter@cisco.com>
//
//  @brief      NHttpMsgChunkHead class declaration
//

#ifndef NHTTP_MSG_CHUNK_HEAD_H
#define NHTTP_MSG_CHUNK_HEAD_H

#include "nhttp_msg_section.h"
#include "nhttp_field.h"

//-------------------------------------------------------------------------
// NHttpMsgChunkHead class
//-------------------------------------------------------------------------

class NHttpMsgChunkHead : public NHttpMsgSection {
public:
    NHttpMsgChunkHead(const uint8_t *buffer, const uint16_t bufSize, NHttpFlowData *sessionData_, NHttpEnums::SourceId sourceId_);
    void analyze();
    void printSection(FILE *output);
    void genEvents();
    void updateFlow();
    void legacyClients();

private:
    void deriveChunkLength();

    Field startLine;
    Field chunkSize;
    Field chunkExtensions;

    int64_t dataLength = NHttpEnums::STAT_NOTCOMPUTE;
    int64_t bodySections;
    int64_t numChunks;
};

#endif

















