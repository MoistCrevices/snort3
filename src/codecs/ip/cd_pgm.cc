//--------------------------------------------------------------------------
// Copyright (C) 2002-2013 Sourcefire, Inc.
// Copyright (C) 2014-2015 Cisco and/or its affiliates. All rights reserved.
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
// cd_pgm.cc author Josh Rosenbaum <jrosenba@cisco.com>



#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "framework/codec.h"
#include "codecs/codec_module.h"
#include "codecs/codec_events.h"
#include "codecs/ip/checksum.h"
#include "protocols/packet.h"

namespace
{

#define CD_PGM_NAME "pgm"
#define CD_PGM_HELP "support for pragmatic general multicast"

static const RuleMap pgm_rules[] =
{
    { DECODE_PGM_NAK_OVERFLOW, "BAD-TRAFFIC PGM nak list overflow attempt" },
    { 0, nullptr }
};

class PgmModule : public CodecModule
{
public:
    PgmModule() : CodecModule(CD_PGM_NAME, CD_PGM_HELP) {}

    const RuleMap* get_rules() const
    { return pgm_rules; }
};


class PgmCodec : public Codec
{
public:
    PgmCodec() : Codec(CD_PGM_NAME){};
    ~PgmCodec() {};

    bool decode(const RawData&, CodecData&, DecodeData&) override;
    void get_protocol_ids(std::vector<uint16_t>&) override;
};


static const uint16_t IPPROTO_ID_PGM = 113;
static const int PGM_NAK_ERR = -1;
static const int PGM_NAK_OK = 0;
static const int PGM_NAK_VULN = 1;

struct PGM_NAK_OPT
{
    uint8_t type;     /* 02 = vuln */
    uint8_t len;
    uint8_t res[2];
    uint32_t seq[1];    /* could be many many more, but 1 is sufficient */
};

struct PGM_NAK
{
    uint32_t  seqnum;
    uint16_t  afil1;
    uint16_t  res1;
    uint32_t  src;
    uint16_t  afi2;
    uint16_t  res2;
    uint32_t  multi;
    PGM_NAK_OPT opt;
};

struct PgmHeader
{
    uint16_t srcport;
    uint16_t dstport;
    uint8_t  type;
    uint8_t  opt;
    uint16_t checksum;
    uint8_t  gsd[6];
    uint16_t length;
    PGM_NAK  nak;
};


} // namespace

/* This PGM NAK function started off as an SO rule, sid 8351. */
static inline int pgm_nak_detect (const RawData& raw)
{
    /* request must be bigger than 44 bytes to cause vuln */
    if (raw.len <= sizeof(PgmHeader)) {
        return PGM_NAK_ERR;
    }

     const PgmHeader* const header =
        reinterpret_cast<const PgmHeader* const>(raw.data);

    if (8 != header->type)
        return PGM_NAK_ERR;

    if (2 != header->nak.opt.type)
        return PGM_NAK_ERR;


    /*
     * alert if the amount of data after the options is more than the length
     * specified.
     */
    const uint16_t data_left = raw.len - 36;

    if (data_left > header->nak.opt.len) {

        /* checksum is expensive... do that only if the length is bad */
        if (header->checksum != 0)
        {
            const uint16_t checksum =
                checksum::cksum_add((uint16_t*)raw.data, raw.len);

            if (checksum != 0)
                return PGM_NAK_ERR;
        }

        return PGM_NAK_VULN;
    }

    return PGM_NAK_OK;
}


//-------------------------------------------------------------------------
// private functions
//-------------------------------------------------------------------------

bool PgmCodec::decode(const RawData& raw, CodecData& codec, DecodeData&)
{
    if ( pgm_nak_detect(raw) == PGM_NAK_VULN )
        codec_events::decoder_event(codec, DECODE_PGM_NAK_OVERFLOW);
    return true;
}

void PgmCodec::get_protocol_ids(std::vector<uint16_t>& v)
{
    v.push_back(IPPROTO_ID_PGM);
}



//-------------------------------------------------------------------------
// api
//-------------------------------------------------------------------------

static Module* mod_ctor()
{ return new PgmModule; }

static void mod_dtor(Module* m)
{ delete m; }

static Codec* ctor(Module*)
{ return new PgmCodec(); }

static void dtor(Codec *cd)
{ delete cd; }


static const CodecApi pgm_api =
{
    {
        PT_CODEC,
        CD_PGM_NAME,
        CD_PGM_HELP,
        CDAPI_PLUGIN_V0,
        0,
        mod_ctor,
        mod_dtor,
    },
    nullptr, // pinit
    nullptr, // pterm
    nullptr, // tinit
    nullptr, // tterm
    ctor, // ctor
    dtor, // dtor
};

#ifdef BUILDING_SO
SO_PUBLIC const BaseApi* snort_plugins[] =
{
    &pgm_api.base,
    nullptr
};
#else
const BaseApi* cd_pgm = &pgm_api.base;
#endif

