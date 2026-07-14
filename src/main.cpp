#include <contracts>
#include <iostream>
#include <print>
#include <ranges>
#include <stacktrace>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <vector>

#include <beman/cstring_view/cstring_view.hpp>

#include "commands/cat.h"
#include "commands/grep.h"

using namespace std::literals;

namespace
{

[[maybe_unused]] auto handle_contract_violation(const std::contracts::contract_violation &cv) -> void
{
    std::println(std::cerr, "contract violation {}", cv.comment());
    std::println(std::cerr, "{}", std::stacktrace::current(2));
}

constexpr auto exe_trunk(::beman::cstring_view exe) noexcept -> ::beman::cstring_view
{
    const auto last_slash = exe.find_last_of("/"sv);
    return last_slash == ::beman::cstring_view::npos ? exe : exe.substr(last_slash + 1zu);
}

constexpr auto convert_raw_args(const int argc, char **argv) noexcept
    -> std::tuple<::beman::cstring_view, std::vector<::beman::cstring_view>> //
    pre(argc > 1)                                                            //
    post(r : std::ranges::size(std::get<1>(r)) == static_cast<std::size_t>(argc - 1))
{
    const auto all_argv =
        std::span(argv, argv + argc) | std::views::transform([](const auto *e) -> ::beman::cstring_view { return e; });

    return std::make_tuple(exe_trunk(all_argv[0]), all_argv | std::views::drop(1zu) | std::ranges::to<std::vector>());
}

}

auto main(int argc, char **argv) -> int
{
    try
    {
        const auto [name, args] = convert_raw_args(argc, argv);
        const auto config = om::load_config();

        if (name == "cat"sv || name.starts_with("ocat"))
        {
            om::cat(config, args);
        }
        else if (name == "grep"sv || name.starts_with("ogrep"))
        {
            om::grep(config, args);
        }
        else
        {
            std::println("omnipp");
        }
    }
    catch (const std::runtime_error &e)
    {
        std::println("{}", e.what());
        std::println(std::cerr, "{}", std::stacktrace::current());
    }
    catch (...)
    {
        std::println("unknown exception");
        std::println(std::cerr, "{}", std::stacktrace::current());
    }

    return 0;
}
