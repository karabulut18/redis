#pragma once

#include <fcntl.h>

namespace fd_util
{
    bool fd_set_nonblock(int fd)
    {
        int flags = fcntl(fd, F_GETFL, 0);
        if (flags < 0)
            return false;
    
        if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
            return false;
        return true;
    }
}