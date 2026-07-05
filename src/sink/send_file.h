#pragma once

#include <cstddef>
#include <cstdio>
#include <expected>
#include <iostream>
#include <span>
#include <string>

#include <sys/sendfile.h>

#include "concepts/concepts.h"

namespace om::sink
{

struct SendFile
{
    using is_sink = bool;

    [[nodiscard]] auto operator()(int fd, std::size_t size, std::size_t offset) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        auto write_amount = std::size_t{};
        while (write_amount < size)
        {
            auto off = static_cast<::off_t>(offset);
            const auto written_this_iter = ::sendfile(STDOUT_FILENO, fd, &off, size);
            if (written_this_iter == -1)
            {
                return std::unexpected(std::format("failed to write: [{} {} ", size, offset));
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

static_assert(Sink<SendFile>);

}
