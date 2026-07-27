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

namespace impl
{
inline thread_local std::string buffer;
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
        auto write_amount = std::size_t{};

        impl::buffer.append_range(data);
        if (force_newline)
        {
            impl::buffer += "\n";
        }

        if (std::ranges::size(impl::buffer) > 4096zu)
        {
            while (write_amount < std::ranges::size(impl::buffer))
            {
                const auto written_this_iter = ::write(
                    STDOUT_FILENO,
                    std::ranges::data(impl::buffer) + write_amount,
                    std::ranges::size(impl::buffer) - write_amount);
                if (written_this_iter == -1)
                {
                    return std::unexpected(
                        std::format(
                            "failed to write: [{}/{} bytes] {}",
                            write_amount,
                            std::ranges::size(impl::buffer),
                            strerror(errno)));
                }

                write_amount += written_this_iter;
            }

            impl::buffer.clear();
        }

        return write_amount;
    }

    auto operator()() const noexcept
    {
        auto write_amount = std::size_t{};
        while (write_amount < std::ranges::size(impl::buffer))
        {
            const auto written_this_iter = ::write(
                STDOUT_FILENO,
                std::ranges::data(impl::buffer) + write_amount,
                std::ranges::size(impl::buffer) - write_amount);
            if (written_this_iter == -1)
            {
                return;
            }

            write_amount += written_this_iter;
        }
    }
};

struct Write : BaseSink<WriteNode>
{
};

static_assert(IsSink<Write>);
}
