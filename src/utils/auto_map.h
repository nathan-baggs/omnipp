#pragma once

#include <cstddef>

#include <sys/mman.h>

namespace om
{
struct AutoMap
{
    ~AutoMap()
    {
        if (map)
        {
            ::munmap(map, size);
        }
    }
    void *map;
    std::size_t size;
};
}
