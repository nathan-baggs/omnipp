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
constexpr auto static_buffer_size = 10zu * 1024zu * 1024zu;
constexpr auto huge_page_alignment = 2u * 1024u * 1024u;
alignas(huge_page_alignment) static auto static_buffer = std::array<std::byte, static_buffer_size>{};
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

        auto dynamic_buffer = std::unique_ptr<std::byte[]>{};
        auto buffer_span = std::span<std::byte>{};

        const auto size = stx.stx_size;
        if (size > impl::static_buffer_size)
        {
            dynamic_buffer.reset(new std::byte[size]);
            if (!dynamic_buffer)
            {
                return std::unexpected("failed to allocate dynamic buffer"s);
            }

            buffer_span = {dynamic_buffer.get(), size};
        }
        else
        {
            buffer_span = impl::static_buffer;
        }

        auto read_amount = std::size_t{};
        while (read_amount != size)
        {
            const auto read_this_iter =
                ::pread(fd, std::ranges::data(buffer_span) + read_amount, size - read_amount, read_amount);
            if (read_this_iter == -1)
            {
                return std::unexpected(std::format("failed to read: {} [{}/{} bytes]", path, read_amount, size));
            }

            read_amount += read_this_iter;
        }

        return N{}(std::span{std::ranges::data(buffer_span), size});
    }
};

static_assert(Source<ReadFile>);

}
