#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <ranges>
#include <string>

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "concepts/concepts.h"
#include "utils/auto_release.h"
#include "utils/fd_closer.h"

using namespace std::literals;

namespace om::source
{

namespace impl
{
}

struct ZeroCopyRead
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(int fd, std::size_t size) const noexcept -> std::expected<std::size_t, std::string>
    {
        constexpr static auto chunk_size = 10zu * 1024zu * 1024zu;

        auto bytes_output = std::size_t{};
        auto offset = std::size_t{};

        while (offset != size)
        {
            const auto read_this_iter = std::min(chunk_size, size - offset);

            const auto processed = N{}(fd, read_this_iter, offset);
            if (!processed)
            {
                return std::unexpected(processed.error());
            }

            offset += read_this_iter;
            bytes_output += *processed;
        }

        return bytes_output;
    }
};

static_assert(Source<ZeroCopyRead>);

}
