#pragma once

#include <cstddef>
#include <cstring>
#include <expected>
#include <format>
#include <mutex>
#include <span>
#include <string>

#include <unistd.h>

#include "concepts/concepts.h"

namespace om::sink
{
namespace impl
{
inline static auto mutex = std::mutex{};
}

template <class T>
struct WriteNode
{
    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        const auto str =
            std::string_view(reinterpret_cast<const char *>(std::ranges::data(data)), std::ranges::size(data));

        return (*this)(str);
    }

    [[nodiscard]] auto operator()(std::string_view data, bool force_newline = false) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        auto lock = std::unique_lock{impl::mutex};

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

        if (force_newline)
        {
            static const auto new_line = '\n';
            ::write(STDOUT_FILENO, &new_line, 1);
            ++write_amount;
        }

        return write_amount;
    }
};

struct Write : BaseSink<WriteNode>
{
};

static_assert(IsSink<Write>);

}
