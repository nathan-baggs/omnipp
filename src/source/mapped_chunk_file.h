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

struct MappedChunkFile
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(int fd, std::size_t size) const noexcept -> std::expected<std::size_t, std::string>
    {
        auto *map_ptr = ::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        if (map_ptr == MAP_FAILED)
        {
            return std::unexpected{std::format("failed to map file: {}", ::strerror(errno))};
        }

        const auto auto_map = AutoMap{map_ptr, size};

        const auto *begin = reinterpret_cast<const std::byte *>(map_ptr);
        const auto map_span = std::span(begin, size);

        volatile auto dummy = std::uint8_t{};
        auto bytes_output = std::size_t{};

        constexpr static auto chunk_size = 10zu * 1024zu * 1024zu;

        for (const auto chunk : map_span | std::views::chunk(chunk_size))
        {
            for (auto i = 0zu; i < std::ranges::size(chunk); i += 4096u)
            {
                dummy += static_cast<std::uint8_t>(chunk[i]);
            }

            const auto processed = N{}(chunk);
            if (!processed)
            {
                return std::unexpected(processed.error());
            }

            bytes_output += *processed;
        }

        return bytes_output;
    }
};

static_assert(Source<MappedChunkFile>);

}
