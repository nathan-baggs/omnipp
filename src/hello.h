#pragma once
#include <string_view>

namespace hello
{
inline auto get_message() -> std::string_view
{
    return "hello world";
}
}
