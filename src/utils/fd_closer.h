#pragma once

#include <unistd.h>

namespace om
{
struct FdCloser
{
    auto operator()(int fd)
    {
        ::close(fd);
    }
};
}
