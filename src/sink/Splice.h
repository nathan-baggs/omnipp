#pragma once

#include <cstddef>
#include <cstdio>
#include <expected>
#include <iostream>
#include <span>
#include <string>

#include <fcntl.h>
#include <unistd.h>

#include "concepts/concepts.h"

using namespace std::literals;

namespace om::sink
{

struct Splice
{
    using is_sink = bool;

    [[nodiscard]] auto operator()(int fd, std::size_t size, std::size_t) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        auto write_amount = std::size_t{};
        while (write_amount < size)
        {
            const auto written_this_iter = ::splice(fd, nullptr, STDOUT_FILENO, nullptr, size, SPLICE_F_MORE);
            if (written_this_iter == -1)
            {
                return std::unexpected{"failed to splice"s};
            }
            else if (written_this_iter == 0)
            {
                break;
            }

            write_amount += written_this_iter;
        }

        return write_amount;
    }
};

static_assert(Sink<Splice>);

}
