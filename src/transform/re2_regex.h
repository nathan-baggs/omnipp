#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <span>
#include <string>

#include <beman/cstring_view/cstring_view.hpp>
#include <re2/re2.h>
#include <re2/regexp.h>

#include "concepts/concepts.h"

namespace om::transform
{

namespace impl
{
}

struct Re2State
{
    Re2State(::beman::cstring_view pattern)
        : regex(std::make_unique<::RE2>(std::string{"(?m)"} + std::string{pattern}))

    {
    }

    Re2State(const Re2State &) = delete;
    Re2State &operator=(const Re2State &) = delete;
    Re2State(Re2State &&) = default;
    Re2State &operator=(Re2State &&) = default;

    std::unique_ptr<::RE2> regex;
};

template <class Next>
struct Re2RegexNode
{
    Re2RegexNode(Next next, Re2State state)
        : next{std::move(next)}
        , state{std::move(state)}
    {
    }

    [[nodiscard]] auto operator()(std::span<const std::byte> data) noexcept -> std::expected<std::size_t, std::string>
    {
        const auto str = std::string_view(reinterpret_cast<const char *>(data.data()), data.size());

        try
        {
            auto found = 0zu;
            auto remaining = str;

            while (!remaining.empty())
            {
                const auto newline_pos = remaining.find('\n');
                const auto line = remaining.substr(0zu, newline_pos);

                if (!line.empty() && ::RE2::PartialMatch(::re2::StringPiece{line.data(), line.size()}, *state.regex))
                {
                    auto next_res = next(line);
                    if (!next_res)
                    {
                        return std::unexpected(next_res.error());
                    }

                    next_res = next("\n");
                    if (!next_res)
                    {
                        return std::unexpected(next_res.error());
                    }

                    ++found;
                }

                if (newline_pos == std::string_view::npos)
                {
                    break;
                }

                remaining = remaining.substr(newline_pos + 1zu);
            }

            return found;
        }

        catch (const std::exception &e)
        {
            return std::unexpected(e.what());
        }
    }

    Next next;
    Re2State state;
};

struct Re2Regex : BaseTransform<Re2RegexNode, Re2State>
{
};

static_assert(IsTransform<Re2Regex>);

}
