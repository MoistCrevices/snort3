//--------------------------------------------------------------------------
// Copyright (C) 2014-2015 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2002-2013 Sourcefire, Inc.
// Copyright (C) 1998-2002 Martin Roesch <roesch@sourcefire.com>
// Copyright (C) 2000,2001 Andrew R. Baker <andrewb@uab.edu>
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

/* alert_fast
 *
 * Purpose:  output plugin for fast alerting
 *
 * Arguments:  alert file
 *
 * Effect:
 *
 * Alerts are written to a file in the snort fast alert format
 *
 * Comments:   Allows use of fast alerts with other output plugin types
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>

#include "framework/logger.h"
#include "framework/module.h"
#include "event.h"
#include "protocols/packet.h"
#include "snort_debug.h"
#include "parser.h"
#include "util.h"
#include "mstring.h"
#include "packet_io/active.h"
#include "log/text_log.h"
#include "log/log_text.h"
#include "snort.h"
#include "packet_io/sfdaq.h"
#include "packet_io/intf.h"
#include "events/event.h"

/* full buf was chosen to allow printing max size packets
 * in hex/ascii mode:
 * each byte => 2 nibbles + space + ascii + overhead
 */
#define FULL_BUF (4*IP_MAXPACKET)
#define FAST_BUF (4*K_BYTES)

static THREAD_LOCAL TextLog* fast_log = nullptr;

using namespace std;

#define S_NAME "alert_fast"
#define F_NAME S_NAME ".txt"

//-------------------------------------------------------------------------
// module stuff
//-------------------------------------------------------------------------

static const Parameter s_params[] =
{
    { "file", Parameter::PT_BOOL, nullptr, "false",
      "output to " F_NAME " instead of stdout" },

    { "packet", Parameter::PT_BOOL, nullptr, "false",
      "output packet dump with alert" },

    { "limit", Parameter::PT_INT, "0:", "0",
      "set limit (0 is unlimited)" },

    { "units", Parameter::PT_ENUM, "B | K | M | G", "B",
      "bytes | KB | MB | GB" },

    { nullptr, Parameter::PT_MAX, nullptr, nullptr, nullptr }
};

#define s_help \
    "output event with brief text format"

class FastModule : public Module
{
public:
    FastModule() : Module(S_NAME, s_help, s_params) { };

    bool set(const char*, Value&, SnortConfig*) override;
    bool begin(const char*, int, SnortConfig*) override;
    bool end(const char*, int, SnortConfig*) override;

public:
    bool file;
    unsigned long limit;
    unsigned units;
    bool packet;
};

bool FastModule::set(const char*, Value& v, SnortConfig*)
{
    if ( v.is("file") )
        file = v.get_bool();

    else if ( v.is("packet") )
        packet = v.get_bool();

    else if ( v.is("limit") )
        limit = v.get_long();

    else if ( v.is("units") )
        units = v.get_long();

    else
        return false;

    return true;
}

bool FastModule::begin(const char*, int, SnortConfig*)
{
    file = false;
    limit = 0;
    units = 0;
    packet = false;
    return true;
}

bool FastModule::end(const char*, int, SnortConfig*)
{
    while ( units-- )
        limit *= 1024;

    return true;
}

//-------------------------------------------------------------------------
// logger stuff
//-------------------------------------------------------------------------

class FastLogger : public Logger {
public:
    FastLogger(FastModule*);

    void open() override;
    void close() override;

    void alert(Packet*, const char* msg, Event*) override;

private:
    string file;
    unsigned long limit;
    bool packet;
};

FastLogger::FastLogger(FastModule* m)
{
    file = m->file ? F_NAME : "stdout";
    limit = m->limit;
    packet = m->packet;
}

void FastLogger::open()
{
    unsigned sz = packet ? FULL_BUF : FAST_BUF;
    fast_log = TextLog_Init(file.c_str(), sz, limit);
}

void FastLogger::close()
{
    if ( fast_log )
        TextLog_Term(fast_log);
}

#ifdef REG_TEST
static void LogReassembly(const Packet* p)
{
    /* Log whether or not this is reassembled data - only indicate
     * if we're actually going to show any of the payload */
    if ( !ScOutputAppData() || !p->dsize || !PacketWasCooked(p) )
        return;

    switch ( p->pseudo_type )
    {
    case PSEUDO_PKT_SMB_SEG:
        TextLog_Print(fast_log, "%s\n", "SMB desegmented packet");
        break;
    case PSEUDO_PKT_DCE_SEG:
        TextLog_Print(fast_log, "%s\n", "DCE/RPC desegmented packet");
        break;
    case PSEUDO_PKT_DCE_FRAG:
        TextLog_Print(fast_log, "%s\n", "DCE/RPC defragmented packet");
        break;
    case PSEUDO_PKT_SMB_TRANS:
        TextLog_Print(fast_log, "%s\n", "SMB Transact reassembled packet");
        break;
    case PSEUDO_PKT_DCE_RPKT:
        TextLog_Print(fast_log, "%s\n", "DCE/RPC reassembled packet");
        break;
    case PSEUDO_PKT_TCP:
        TextLog_Print(fast_log, "%s\n", "Stream reassembled packet");
        break;
    case PSEUDO_PKT_IP:
        TextLog_Print(fast_log, "%s\n", "Frag reassembled packet");
        break;
    default:
        // FIXTHIS do we get here for portscan or sdf?
        break;
    }
}
#endif

#ifndef REG_TEST

static const char* get_pkt_type(Packet* p)
{
    switch ( p->ptrs.get_pkt_type() )
    {
    case PktType::IP:   return "IP";
    case PktType::ICMP: return "ICMP";
    case PktType::TCP:  return "TCP";
    case PktType::UDP:  return "UDP";
    default: break;
    }
    return "error";
}

#endif

void FastLogger::alert(Packet *p, const char *msg, Event *event)
{
    LogTimeStamp(fast_log, p);

    if( Active_PacketWasDropped() )
    {
        TextLog_Puts(fast_log, " [Drop]");
    }
    else if( Active_PacketWouldBeDropped() )
    {
        TextLog_Puts(fast_log, " [WDrop]");
    }

    {
#ifdef MARK_TAGGED
        char c=' ';
        if (p->packet_flags & PKT_REBUILT_STREAM)
            c = 'R';
        else if (p->packet_flags & PKT_REBUILT_FRAG)
            c = 'F';
        TextLog_Print(fast_log, " [**] %c ", c);
#else
        TextLog_Puts(fast_log, " [**] ");
#endif

        if(event != NULL)
        {
            TextLog_Print(fast_log, "[%lu:%lu:%lu] ",
                    (unsigned long) event->sig_info->generator,
                    (unsigned long) event->sig_info->id,
                    (unsigned long) event->sig_info->rev);
        }

        if (ScAlertInterface())
        {
            TextLog_Print(fast_log, " <%s> ", PRINT_INTERFACE(DAQ_GetInterfaceSpec()));
        }

        if (msg != NULL)
        {
#ifdef REG_TEST
            string tmp = msg + 1;
            tmp.pop_back();
            TextLog_Puts(fast_log, tmp.c_str());
#else
            TextLog_Puts(fast_log, msg);
#endif
        }
        TextLog_Puts(fast_log, " [**] ");
    }

    /* print the packet header to the alert file */
    if ( p->has_ip() )
    {
        LogPriorityData(fast_log, event, 0);
#ifndef REG_TEST
        TextLog_Print(fast_log, "{%s} ", get_pkt_type(p));
#else
        TextLog_Print(fast_log, "{%s} ", protocol_names[p->get_ip_proto_next()]);
#endif
        LogIpAddrs(fast_log, p);
    }

    if ( packet || ScOutputAppData() )
    {
        TextLog_NewLine(fast_log);
#ifdef REG_TEST
        LogReassembly(p);
#endif
        if(p->has_ip())
            LogIPPkt(fast_log, p);

#if 0
        // FIXIT-L -J LogArpHeader unimplemented
        else if(p->proto_bits & PROTO_BIT__ARP)
            LogArpHeader(fast_log, p);
#endif
    }
    TextLog_NewLine(fast_log);
    TextLog_Flush(fast_log);
}

//-------------------------------------------------------------------------
// api stuff
//-------------------------------------------------------------------------

static Module* mod_ctor()
{ return new FastModule; }

static void mod_dtor(Module* m)
{ delete m; }

static Logger* fast_ctor(SnortConfig*, Module* mod)
{ return new FastLogger((FastModule*)mod); }

static void fast_dtor(Logger* p)
{ delete p; }

static LogApi fast_api
{
    {
        PT_LOGGER,
        S_NAME,
        s_help,
        LOGAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor
    },
    OUTPUT_TYPE_FLAG__ALERT,
    fast_ctor,
    fast_dtor
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &fast_api.base,
    nullptr
};
#else
const BaseApi* alert_fast = &fast_api.base;
#endif

