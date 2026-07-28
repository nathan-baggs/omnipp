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
#include "utils/auto_map.h"
#include "utils/auto_release.h"
#include "utils/fd_closer.h"

namespace om::source
{

template <class Next>
struct MappedFileNode
{
    [[nodiscard]] auto operator()(int fd, std::size_t size) noexcept -> std::expected<std::size_t, std::string>
    {
        auto *map_ptr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (!map_ptr)
        {
            return std::unexpected{std::format("failed to map file: {}", ::strerror(errno))};
        }

        const auto auto_map = AutoMap{map_ptr, size};

        const auto *begin = reinterpret_cast<const std::byte *>(map_ptr);
        return next(std::span(begin, begin + size));
    }

    Next next;
};

struct MappedFile : BaseSource<MappedFileNode>
{
};

static_assert(IsSource<MappedFile>);
}
