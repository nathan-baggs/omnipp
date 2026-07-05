#pragma once

#include <cstddef>
#include <expected>
#include <iostream>
#include <span>
#include <string>

#include "concepts/concepts.h"

namespace om::sink
{

struct Cout
{
    using is_sink = bool;

    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        const auto str =
            std::string_view(reinterpret_cast<const char *>(std::ranges::data(data)), std::ranges::size(data));
        std::cout << str;

        return std::ranges::size(data);
    }
};

static_assert(Sink<Cout>);

}
