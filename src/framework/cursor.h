//--------------------------------------------------------------------------
// Copyright (C) 2014-2015 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2002-2013 Sourcefire, Inc.
// Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
// cursor.h author Russ Combs <rucombs@cisco.com>

#ifndef CURSOR_H
#define CURSOR_H

#include <ctype.h>
#include <stdint.h>
#include <string.h>

struct Packet;

class Cursor
{
public:
    Cursor(Packet*);
    Cursor(const Cursor&);

    const char* get_name() const
    { return name; };

    bool is(const char* s) const
    { return !strcmp(name, s); };

    void reset(Packet*);

    void set(const char* s, const uint8_t* b, unsigned n)
    { name = s; data = b; sz = n; pos = delta = 0; };

    const uint8_t* buffer() const
    { return data; };

    unsigned size() const
    { return sz; };

    // the NEXT octect after last in buffer
    // (this pointer is out of bounds)
    const uint8_t* endo() const
    { return data + sz; };

    const uint8_t* start() const
    { return data + pos; };

    unsigned length() const
    { return sz - pos; };

    unsigned get_pos() const
    { return pos; };

    unsigned get_delta() const
    { return delta; };

    bool add_pos(unsigned n)
    { 
        if (pos + n > sz)
            return false;
        pos += n;
        return true;
    };

    // pos and delta may go 1 byte after end
    bool set_pos(unsigned n)
    { 
        if (n > sz)
            return false;
        pos = n;
        return true;
    };

    bool set_delta(unsigned n)
    {
        if (n > sz)
            return false;
        delta = n;
        return true;
    };

private:
    const char* name;     // rule option name ("pkt_data", "http_uri", etc.)
    const uint8_t* data;  // start of buffer
    unsigned sz;          // size of buffer
    unsigned pos;         // current pos
    unsigned delta;       // loop offset

};

#endif

