#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <print>
#include <span>
#include <string>

#include <beman/cstring_view/cstring_view.hpp>
#include <hs.h>

#include "concepts/concepts.h"
#include "hs_common.h"
#include "hs_compile.h"
#include "hs_runtime.h"

using namespace std::literals;

namespace om::transform
{

namespace impl
{

struct Match
{
    std::size_t end;
};

struct Context
{
    std::vector<Match> matches;
};

inline auto on_match(unsigned int, unsigned long long, unsigned long long to, unsigned int, void *context) -> int
{
    auto *ctx = static_cast<Context *>(context);
    ctx->matches.emplace_back(to);

    return 0;
}

}

struct VectorScanState
{
    inline static const auto hs_database_free = [](::hs_database_t *db) { ::hs_free_database(db); };
    inline static const auto hs_scratch_free = [](::hs_scratch_t *s) { ::hs_free_scratch(s); };

    VectorScanState(::beman::cstring_view regex)
        : db{}
        , scratch{}
    {
        {
            static const auto hs_compiler_error_free = [](::hs_compile_error_t *err) { ::hs_free_compile_error(err); };
            auto compiler_error = std::unique_ptr<::hs_compile_error_t, decltype(hs_compiler_error_free)>{};
            const auto res = ::hs_compile(
                regex.c_str(),
                HS_FLAG_MULTILINE,
                HS_MODE_BLOCK,
                nullptr,
                std::inout_ptr(db),
                std::inout_ptr(compiler_error));

            if (res != HS_SUCCESS)
            {
                throw std::runtime_error(
                    std::format("failed to compile regex: {} [{}]", regex, compiler_error->message));
            }
        }

        {
            const auto res = ::hs_alloc_scratch(db.get(), std::inout_ptr(scratch));
            if (res != HS_SUCCESS)
            {
                throw std::runtime_error(std::format("failed to create scratch space: {}", res));
            }
        }
    }

    ~VectorScanState() = default;
    VectorScanState(const VectorScanState &) = delete;
    auto operator=(const VectorScanState &) -> VectorScanState & = delete;
    VectorScanState(VectorScanState &&) = default;
    auto operator=(VectorScanState &&) -> VectorScanState & = default;

    std::unique_ptr<::hs_database_t, decltype(hs_database_free)> db;
    std::unique_ptr<::hs_scratch_t, decltype(hs_scratch_free)> scratch;
};

template <class Next>
struct VectorScanNode
{
    VectorScanNode(Next next, VectorScanState state)
        : next{std::move(next)}
        , state{std::move(state)}
    {
    }

    ~VectorScanNode() = default;
    VectorScanNode(const VectorScanNode &) = delete;
    auto operator=(const VectorScanNode &) -> VectorScanNode & = delete;
    VectorScanNode(VectorScanNode &&) = default;
    auto operator=(VectorScanNode &&) -> VectorScanNode & = default;

    [[nodiscard]] auto operator()(std::span<const std::byte> data) noexcept -> std::expected<std::size_t, std::string>
    {
        const auto *data_str = reinterpret_cast<const char *>(std::ranges::data(data));
        const auto str = std::string_view{data_str, std::ranges::size(data)};

        try
        {
            auto ctx = impl::Context{};

            const auto res = ::hs_scan(
                state.db.get(), data_str, std::ranges::size(data), 0, state.scratch.get(), impl::on_match, &ctx);

            if (res != HS_SUCCESS)
            {
                return std::unexpected("failed to parse input");
            }

            for (const auto &match : ctx.matches)
            {
                const auto match_end = match.end;
                const auto previous_newline = str.substr(0zu, match_end).find_last_of('\n');
                const auto line_start = previous_newline == std::string_view::npos ? 0zu : previous_newline + 1zu;
                const auto next_newline = str.find('\n', match_end);
                const auto line_end = next_newline == std::string_view::npos ? str.size() : next_newline;

                auto line = str.substr(line_start, line_end - line_start);

                if (!line.empty() && line.back() == '\r')
                {
                    line.remove_suffix(1zu);
                }

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
            }

            return std::ranges::size(ctx.matches);
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
    VectorScanState state;
};

struct VectorScan : BaseTransform<VectorScanNode, VectorScanState>
{
};

static_assert(IsTransform<VectorScan>);

}
