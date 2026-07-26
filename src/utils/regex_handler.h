#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>
#include <hs.h>

using namespace std::literals;

namespace om
{

namespace impl
{

struct Match
{
    std::size_t begin;
    std::size_t end;
};

struct Context
{
    std::vector<Match> matches;
};

inline auto on_match(unsigned int, unsigned long long from, unsigned long long to, unsigned int, void *context) -> int
{
    auto *ctx = static_cast<Context *>(context);
    ctx->matches.emplace_back(from, to);

    return 0;
}
}

template <class EV>
struct RegexHandler
{
    RegexHandler(EV &ev, ::beman::cstring_view regex)
        : ev{ev}
        , db{}
        , scratch{}
    {
        {
            static const auto hs_compiler_error_free = [](::hs_compile_error_t *err) { ::hs_free_compile_error(err); };
            auto compiler_error = std::unique_ptr<::hs_compile_error_t, decltype(hs_compiler_error_free)>{};
            const auto res = ::hs_compile(
                regex.c_str(),
                HS_FLAG_MULTILINE | HS_FLAG_SOM_LEFTMOST,
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

    auto operator()(std::span<const std::byte> data) -> std::expected<std::size_t, std::string>
    {
        const auto *data_str = reinterpret_cast<const char *>(std::ranges::data(data));

        try
        {
            auto ctx = impl::Context{};

            const auto res =
                ::hs_scan(db.get(), data_str, std::ranges::size(data), 0, scratch.get(), impl::on_match, &ctx);

            if (res != HS_SUCCESS)
            {
                return std::unexpected("failed to parse input");
            }

            for (const auto &[begin, end] : ctx.matches)
            {
                auto line = std::string_view(data_str + begin, data_str + end);
                while (!line.empty() && (line.front() == '\n' || line.front() == '\r'))
                {
                    line.remove_prefix(1zu);
                }

                auto line_str = std::string{line};
                line_str += '\n';

                ev.queue_write(STDOUT_FILENO, std::move(line_str));
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

    inline static const auto hs_database_free = [](::hs_database_t *db) { ::hs_free_database(db); };
    inline static const auto hs_scratch_free = [](::hs_scratch_t *s) { ::hs_free_scratch(s); };

    EV &ev;
    std::unique_ptr<::hs_database_t, decltype(hs_database_free)> db;
    std::unique_ptr<::hs_scratch_t, decltype(hs_scratch_free)> scratch;
};

}
