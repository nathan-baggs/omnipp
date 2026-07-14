#pragma once

#include <cstddef>
#include <expected>
#include <iostream>
#include <span>
#include <string>

#include "concepts/concepts.h"

namespace om::sink
{

template <class Next>
struct CoutNode
{
    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        const auto str =
            std::string_view(reinterpret_cast<const char *>(std::ranges::data(data)), std::ranges::size(data));

        return (*this)(str);
    }

    [[nodiscard]] auto operator()(std::string_view data) const noexcept -> std::expected<std::size_t, std::string>
    {
        std::cout << data;

        return std::ranges::size(data);
    }
};

struct Cout : BaseSink<CoutNode>
{
};

static_assert(IsSink<Cout>);

}
