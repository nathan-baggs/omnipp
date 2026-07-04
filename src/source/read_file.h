#pragma once

#include <cstddef>
#include <expected>
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

namespace om::source
{

namespace impl
{
constexpr auto static_buffer_size = 2zu * 1024zu * 1024zu;
alignas(static_buffer_size) static auto static_buffer = std::array<std::byte, static_buffer_size>{};
}

struct ReadFile
{
    using is_source = bool;

    template <class N>
    [[nodiscard]] auto operator()(::beman::cstring_view path) const noexcept -> std::expected<std::size_t, std::string>
    {
        const auto fd = AutoRelease<int, FdCloser, -1>{::openat(AT_FDCWD, path.c_str(), O_RDONLY)};
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
        if (size > impl::static_buffer_size)
        {
            return std::unexpected(
                std::format("file too large for this source: {} > {}", size, impl::static_buffer_size));
        }

        auto read_amount = std::size_t{};
        while (read_amount != size)
        {
            const auto read_this_iter =
                ::pread(fd, std::ranges::data(impl::static_buffer) + read_amount, size - read_amount, read_amount);
            if (read_this_iter == -1)
            {
                return std::unexpected(std::format("failed to read: {} [{}/{} bytes]", path, read_amount, size));
            }

            read_amount += read_this_iter;
        }

        return N{}(std::span{std::ranges::data(impl::static_buffer), size});
    }
};

static_assert(Source<ReadFile>);

}
