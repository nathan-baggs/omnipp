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

struct CopyFileRange
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(int fd, std::size_t size) const noexcept -> std::expected<std::size_t, std::string>
    {
        auto offset = std::size_t{};

        while (offset != size)
        {
            static constexpr auto max_chunk = 1024zu * 1024zu * 1024zu;
            const auto chunk_size = std::min(max_chunk, size - offset);

            const auto copied_this_iter = ::copy_file_range(fd, nullptr, STDOUT_FILENO, nullptr, chunk_size, 0);
            if (copied_this_iter == -1)
            {
                return std::unexpected{"failed to copy_file_range"s};
            }
            else if (copied_this_iter == 0)
            {
                break;
            }

            offset += copied_this_iter;
        }

        return offset;
    }
};

static_assert(Source<CopyFileRange>);

}
