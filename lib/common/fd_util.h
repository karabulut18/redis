#pragma once

#include <fcntl.h>

namespace fd_util
{
    bool fd_set_nonblock(int fd);
}