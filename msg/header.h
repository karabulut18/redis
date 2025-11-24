#pragma once

#include "../lib/frame_header.h"

#define MSG_VERSION 2

namespace msg
{
    struct header
    {
        frame_header   _f_hdr;
        uint32_t       _type;
        uint32_t       _version;
        header(m_size_t size, uint32_t type, uint32_t version) 
            : _f_hdr(size), _type(type), _version(version) {};
    } __attribute__((packed, aligned(8)));
}