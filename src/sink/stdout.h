#pragma once

#include <cstddef>
#include <expected>
#include <iostream>
#include <span>
#include <string>

#include "concepts/concepts.h"

namespace om::sink
{

struct Stdout
{
    using is_sink = bool;

    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        std::cout.write(reinterpret_cast<const char *>(data.data()), data.size());
        return data.size();
    }
};

static_assert(Sink<Stdout>);

}
