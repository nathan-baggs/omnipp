#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <format>
#include <memory>
#include <print>
#include <ranges>
#include <span>
#include <string>
#include <vector>

#include <tree_sitter/api.h>

#include "concepts/concepts.h"

using namespace std::literals;

extern "C" const ::TSLanguage *tree_sitter_cpp();

namespace om
{

namespace impl
{

constexpr const char cpp_query_scm[] = {
#embed "../../third_party/tree-sitter-c/queries/highlights.scm"
    ,
#embed "../../third_party/tree-sitter-cpp/queries/highlights.scm"
};

constexpr std::string_view reset = "\033[0m"sv;
constexpr std::string_view colours[] = {
    ""sv,         // no-colour
    "\033[35m"sv, // magenta
    "\033[34m"sv, // blue
    "\033[33m"sv, // yellow
    "\033[36m"sv  // cyan
};

constexpr auto to_byte_span(std::string_view str) -> std::span<const std::byte>
{
    return {
        reinterpret_cast<const std::byte *>(std::ranges::data(str)),
        reinterpret_cast<const std::byte *>(std::ranges::data(str)) + std::ranges::size(str)};
}

}

class TreeSitterTransform
{
  public:
    [[nodiscard]] auto transform(std::span<const std::byte> data) const noexcept
        -> std::expected<std::vector<std::byte>, std::string>
    {
        const auto *language = tree_sitter_cpp();

        constexpr auto delete_parser = [](auto *p) { ::ts_parser_delete(p); };
        const auto parser = std::unique_ptr<::TSParser, decltype(delete_parser)>{::ts_parser_new()};
        if (!parser)
        {
            return std::unexpected{"failed to create parser"s};
        }

        if (!::ts_parser_set_language(parser.get(), language))
        {
            return std::unexpected{"failed to set language"s};
        }

        auto error_offset = std::uint32_t{};
        auto error_type = ::TSQueryError{};

        constexpr auto delete_query = [](auto *q) { ::ts_query_delete(q); };
        const auto query = std::unique_ptr<::TSQuery, decltype(delete_query)>{::ts_query_new(
            language,
            std::ranges::data(impl::cpp_query_scm),
            std::ranges::size(impl::cpp_query_scm),
            &error_offset,
            &error_type)};
        if (!query || error_type != ::TSQueryErrorNone)
        {
            return std::unexpected{std::format("failed to create query: {}", std::to_underlying(error_type))};
        }

        constexpr auto delete_cursor = [](auto *c) { ::ts_query_cursor_delete(c); };
        const auto cursor = std::unique_ptr<::TSQueryCursor, decltype(delete_cursor)>{::ts_query_cursor_new()};
        if (!cursor)
        {
            return std::unexpected{"failed to create cursor"s};
        }

        const auto capture_count = ::ts_query_capture_count(query.get());
        const auto capture_colour = std::views::iota(0zu, capture_count) |
                                    std::views::transform(
                                        [q = query.get()](auto index)
                                        {
                                            auto length = std::uint32_t{};
                                            const auto *name = ::ts_query_capture_name_for_id(q, index, &length);
                                            const auto name_view = std::string_view{name, length};

                                            if (name_view == "keyword"sv)
                                            {
                                                return 1u;
                                            }
                                            else if (name_view == "function"sv)
                                            {
                                                return 2u;
                                            }
                                            else if (name_view == "string"sv)
                                            {
                                                return 3u;
                                            }
                                            else if (name_view == "variable"sv)
                                            {
                                                return 4u;
                                            }
                                            return 0u;
                                        }) |
                                    std::ranges::to<std::vector>();

        constexpr auto delete_tree = [](auto *t) { ::ts_tree_delete(t); };
        const auto tree = std::unique_ptr<::TSTree, decltype(delete_tree)>{::ts_parser_parse_string(
            parser.get(), nullptr, reinterpret_cast<const char *>(std::ranges::data(data)), std::ranges::size(data))};
        if (!tree)
        {
            return std::unexpected{"failed to parse string"s};
        }

        ::ts_query_cursor_exec(cursor.get(), query.get(), ::ts_tree_root_node(tree.get()));

        auto colour_mask = std::vector<std::uint8_t>(std::ranges::size(data), 0);

        auto match = ::TSQueryMatch{};
        auto capture_index = std::uint32_t{};

        while (::ts_query_cursor_next_capture(cursor.get(), &match, &capture_index))
        {
            const auto &capture = match.captures[capture_index];
            const auto colour_id =
                capture.index < std::ranges::size(capture_colour) ? capture_colour[capture.index] : 0u;

            if (colour_id == 0u)
            {
                continue;
            }

            const auto begin = ::ts_node_start_byte(capture.node);
            const auto end = ::ts_node_end_byte(capture.node);

            std::memset(std::ranges::data(colour_mask) + begin, colour_id, end - begin);
        }

        auto formatted_buffer = std::vector<std::byte>{};
        formatted_buffer.reserve(std::ranges::size(data) * 2zu);

        auto index = std::size_t{};

        for (const auto chunk : colour_mask | std::views::chunk_by(std::equal_to{}))
        {
            const auto colour_id = chunk[0];
            if (colour_id >= std::ranges::size(impl::colours))
            {
                return std::unexpected(std::format("unknown colour id: {}", colour_id));
            }

            formatted_buffer.append_range(impl::to_byte_span(impl::colours[colour_id]));
            formatted_buffer.append_range(data.subspan(index, std::ranges::size(chunk)));

            index += std::ranges::size(chunk);
        }

        formatted_buffer.append_range(impl::to_byte_span(impl::reset));

        return formatted_buffer;
    }

  private:
};

static_assert(Transform<TreeSitterTransform>);

}
