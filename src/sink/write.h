#pragma once

#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <span>
#include <string>

#include <unistd.h>

#include "concepts/concepts.h"

namespace om::sink
{

template <class T>
struct WriteNode
{
    using is_sink = bool;

    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        auto write_amount = std::size_t{};
        while (write_amount < std::ranges::size(data))
        {
            const auto written_this_iter =
                ::write(STDOUT_FILENO, std::ranges::data(data) + write_amount, std::ranges::size(data) - write_amount);
            if (written_this_iter == -1)
            {
                return std::unexpected(
                    std::format(
                        "failed to write: [{}/{} bytes] {}", write_amount, std::ranges::size(data), strerror(errno)));
            }

            write_amount += written_this_iter;
        }

        return write_amount;
    }
};

struct Write : BaseSink<WriteNode>
{
};

static_assert(IsSink<Write>);

}
