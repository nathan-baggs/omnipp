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

template <class Next>
struct DirectSpliceNode
{
    [[nodiscard]] auto operator()(int fd, std::size_t size) const noexcept -> std::expected<std::size_t, std::string>
    {
        const auto pipe_size = ::fcntl(STDOUT_FILENO, F_SETPIPE_SZ, 1024 * 1024);
        if (pipe_size < 0)
        {
            return std::unexpected{std::format("failed to get out pipe size: {}", strerror(errno))};
        }

        auto offset = std::size_t{};

        while (offset != size)
        {
            const auto chunk_size = std::min(static_cast<std::size_t>(pipe_size), size - offset);

            const auto sent_this_iter = ::splice(fd, nullptr, STDOUT_FILENO, nullptr, chunk_size, SPLICE_F_MORE);
            if (sent_this_iter == -1)
            {
                return std::unexpected{"failed to splice data"s};
            }
            else if (sent_this_iter == 0)
            {
                break;
            }

            offset += sent_this_iter;
        }

        return offset;
    }

    Next next;
};

struct DirectSplice : BaseSource<DirectSpliceNode>
{
};

static_assert(IsSource<DirectSplice>);

}
