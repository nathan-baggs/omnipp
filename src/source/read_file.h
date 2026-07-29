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
struct ReadFileNode
{
    constexpr static auto max_size = 10zu * 1024zu * 1024zu;

    [[nodiscard]] auto operator()(int fd, std::size_t size) noexcept -> std::expected<std::size_t, std::string>
    {
        constexpr auto huge_page_alignment = 2u * 1024u * 1024u;
        alignas(huge_page_alignment) thread_local auto static_buffer = std::array<std::byte, max_size>{};

        auto bytes_output = std::size_t{};

        auto read_amount = std::size_t{};
        for (;;)
        {
            const auto read_this_iter = ::pread(fd, std::ranges::data(static_buffer), max_size, read_amount);
            if (read_this_iter == -1)
            {
                return std::unexpected(std::format("failed to read: [{}/{} bytes]", read_amount, size));
            }
            else if (read_this_iter == 0)
            {
                break;
            }

            auto processed = next(std::span(std::ranges::data(static_buffer), read_this_iter));
            if (!processed)
            {
                return std::unexpected(processed.error());
            }

            read_amount += read_this_iter;
            bytes_output += *processed;
        }

        return bytes_output;
    }

    Next next;
};

struct ReadFile : BaseSource<ReadFileNode>
{
};

static_assert(IsSource<ReadFile>);

}
