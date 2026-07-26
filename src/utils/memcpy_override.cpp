#include <cstddef>

#pragma GCC target("ssse3")

#include <dpdk_memcpy.h>

#pragma GCC optimize("O3")
#pragma GCC optimize("-fno-tree-loop-distribute-patterns")

extern "C" void *__wrap_memcpy(void *__restrict dest, const void *__restrict src, std::size_t n)
{
    return ::rte_memcpy(dest, src, n);
}
