#pragma once

#include <cstdlib>

namespace om
{

struct Config
{
    bool colour_output;
};

inline auto load_config() -> Config
{
    return {.colour_output = !(::getenv("NO_COLOUR") || ::getenv("NO_COLOR"))};
}
}
