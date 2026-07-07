#pragma once

#include <cstring>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <beman/cstring_view/cstring_view.hpp>

namespace om
{

inline auto grep([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args) //
    pre(!std::ranges::empty(args))
{
    std::println("grep");
}

}
