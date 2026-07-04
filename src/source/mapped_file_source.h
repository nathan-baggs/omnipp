#pragma once

#include <cstddef>
#include <cstring>
#include <expected>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <tuple>

#include <errno.h>
#include <fcntl.h>
#include <format>
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

}

class MappedFile
{

  public:
    MappedFile(::beman::cstring_view path) noexcept
        : path_{path}
        , map_{}
    {
    }

    ~MappedFile() noexcept
    {
        if (map_)
        {
            const auto [ptr, size] = *map_;
            ::munmap(ptr, size);
        }
    }

    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;
    MappedFile(MappedFile &&) = default;
    MappedFile &operator=(MappedFile &&) = default;

    [[nodiscard]] auto read() noexcept -> std::expected<std::span<const std::byte>, std::string>
    {
        if (map_)
        {
            const auto [ptr, size] = *map_;
            const auto *begin = reinterpret_cast<const std::byte *>(ptr);

            return std::span(begin, begin + size);
        }

        const auto fd = AutoRelease<int, impl::FdCloser, -1>{::openat(AT_FDCWD, path_.c_str(), O_RDONLY)};
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

        map_ = std::make_optional(std::make_tuple(map_ptr, size));

        const auto *begin = reinterpret_cast<const std::byte *>(map_ptr);
        return std::span(begin, begin + size);
    }

  private:
    ::beman::cstring_view path_;
    std::optional<std::tuple<void *, std::size_t>> map_;
};

static_assert(Source<MappedFile>);

}
