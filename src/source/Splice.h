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

struct Splice
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(int fd, std::size_t size) const noexcept -> std::expected<std::size_t, std::string>
    {
        auto pipes = std::array<int, 2zu>{};
        if (::pipe(std::ranges::data(pipes)) != 0)
        {
            return std::unexpected{std::format("failed to create pipes: {}", ::strerror(errno))};
        }

        const auto in_pipe = AutoRelease<int, FdCloser, -1>{pipes[1]};
        const auto out_pipe = AutoRelease<int, FdCloser, -1>{pipes[0]};

        const auto pipe_size = ::fcntl(out_pipe, F_SETPIPE_SZ, 1024 * 1024);
        if (pipe_size < 0)
        {
            return std::unexpected{std::format("failed to get out pipe size: {}", strerror(errno))};
        }

        auto bytes_output = std::size_t{};
        auto offset = std::size_t{};

        while (offset != size)
        {
            const auto chunk_size = std::min(static_cast<std::size_t>(pipe_size), size - offset);

            const auto sent_this_iter = ::splice(fd, nullptr, in_pipe, nullptr, chunk_size, SPLICE_F_MORE);
            if (sent_this_iter == -1)
            {
                return std::unexpected{"failed to splice data in"s};
            }
            else if (sent_this_iter == 0)
            {
                break;
            }

            const auto processed = N{}(out_pipe.get(), chunk_size, 0zu);
            if (!processed)
            {
                return std::unexpected(processed.error());
            }

            offset += sent_this_iter;
            bytes_output += *processed;
        }

        return bytes_output;
    }
};

static_assert(Source<Splice>);

}
