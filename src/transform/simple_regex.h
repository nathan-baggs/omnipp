#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <regex>
#include <span>
#include <string>

#include "concepts/concepts.h"

using namespace std::literals;

namespace om::transform
{

struct State
{
    State(const std::string &regex_str)
        : regex{regex_str, std::regex::ECMAScript | std::regex::multiline}
    {
    }

    std::regex regex;
};

template <class Next>
struct SimpleRegexNode
{
    [[nodiscard]] auto operator()(std::span<const std::byte> data) const noexcept
        -> std::expected<std::size_t, std::string>
    {
        try
        {
            auto str =
                std::string_view{reinterpret_cast<const char *>(std::ranges::data(data)), std::ranges::size(data)};

            auto match = std::match_results<std::string_view::const_iterator>{};

            auto found = std::size_t{};

            while (std::regex_search(std::ranges::begin(str), std::ranges::end(str), match, state.regex))
            {
                const auto previous_newline_str = std::string_view(str.data(), match.position());
                const auto previous_newline = previous_newline_str.find_last_of('\n');
                const auto start =
                    previous_newline == std::string_view::npos ? str.data() : str.data() + previous_newline + 1zu;
                const auto match_end = match.position() + match.length();
                const auto next_newline = str.find('\n', match_end);
                const auto out_end =
                    next_newline == std::string_view::npos ? str.data() + str.size() : str.data() + next_newline;

                auto line = std::string_view(start, out_end);
                while (!line.empty() && (line.front() == '\n' || line.front() == '\r'))
                {
                    line.remove_prefix(1zu);
                }

                const auto res = next(line, true);
                if (!res)
                {
                    return std::unexpected{res.error()};
                }

                ++found;

                if (next_newline == std::string_view::npos)
                {
                    break;
                }

                str = str.subview(next_newline + 1zu);
            }

            return found;
        }
        catch (const std::exception &e)
        {
            return std::unexpected(e.what());
        }
        catch (...)
        {
            return std::unexpected("unknown exception"s);
        }
    }

    Next next;
    State state;
};

struct SimpleRegex : BaseTransform<SimpleRegexNode, State>
{
};

static_assert(IsTransform<SimpleRegex>);

}
