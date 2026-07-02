#include <print>

#include "hello.h"

auto main() -> int
{
    std::println("{}", hello::get_message());
    return 0;
}
