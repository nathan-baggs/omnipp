#pragma once

#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <tuple>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "concepts/concepts.h"
#include "utils/auto_release.h"

namespace om::source
{

namespace impl
{
struct FdCloser
{
    auto operator()(int fd)
    {
        ::close(fd);
    }
};

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

struct MappedFile
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(::beman::cstring_view path) const noexcept -> std::expected<std::size_t, std::string>
    {
        const auto fd = AutoRelease<int, impl::FdCloser, -1>{::openat(AT_FDCWD, path.c_str(), O_RDONLY)};
        if (!fd)
        {
            return std::unexpected{std::format("failed to open file: {}", ::strerror(errno))};
        }

        struct statx stx{};
        if (::statx(fd, "", AT_EMPTY_PATH, STATX_SIZE, &stx) != 0)
        {
            return std::unexpected{std::format("failed to statx file: {}", ::strerror(errno))};
        }

        const auto size = stx.stx_size;

        auto *map_ptr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (!map_ptr)
        {
            return std::unexpected{std::format("failed to map file: {}", ::strerror(errno))};
        }

        const auto auto_map = impl::AutoMap{map_ptr, size};

        const auto *begin = reinterpret_cast<const std::byte *>(map_ptr);
        return N{}(std::span(begin, begin + size));
    }
};

static_assert(Source<MappedFile>);

}
