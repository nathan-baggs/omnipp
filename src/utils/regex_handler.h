#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <expected>
#include <memory>
#include <mutex>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>
#include <hs.h>

#include "utils/request_pool.h"

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
        , to_process{}
        , mutex{}
        , write_mutex{}
        , cv{}
        , running{true}
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

        for (auto i = 0zu; i < std::max(std::jthread::hardware_concurrency() - 1zu, 4zu); ++i)
        {
            workers.emplace_back([this] { worker(); });
        }
    }

    ~RegexHandler()
    {
        running = false;
        cv.notify_all();
    }

    auto worker() -> void
    {
        auto local_scratch = std::unique_ptr<::hs_scratch_t, decltype(hs_scratch_free)>{};
        const auto scratch_res = ::hs_clone_scratch(scratch.get(), std::inout_ptr(local_scratch));
        if (scratch_res != HS_SUCCESS)
        {
            throw std::runtime_error(std::format("failed to create local cratch space: {}", scratch_res));
        }

        auto ctx = impl::Context{};
        thread_local auto write_buffer = std::string{};

        while (running)
        {
            ReadRequest *req{};

            {
                auto lock = std::unique_lock(mutex);
                cv.wait(lock, [this] { return !running || !std::ranges::empty(to_process); });

                if (!running)
                {
                    break;
                }

                req = std::move(to_process.front());
                to_process.pop_front();
            }

            contract_assert(req);

            const auto *data_str = reinterpret_cast<const char *>(std::ranges::data(req->buffer));

            ctx.matches.clear();
            const auto res = ::hs_scan(db.get(), data_str, req->res, 0, local_scratch.get(), impl::on_match, &ctx);

            if (res != HS_SUCCESS)
            {
                throw std::runtime_error("failed to parse input");
            }

            for (const auto &[begin, end] : ctx.matches)
            {
                auto line = std::string_view(data_str + begin, data_str + end);
                while (!line.empty() && (line.front() == '\n' || line.front() == '\r'))
                {
                    line.remove_prefix(1zu);
                }

                write_buffer.append(std::ranges::data(line), std::ranges::size(line));
                write_buffer.append("\n");

                if (std::ranges::size(write_buffer) > 4096zu)
                {
                    auto lock = std::unique_lock{write_mutex};
                    ::write(STDOUT_FILENO, std::ranges::data(write_buffer), std::ranges::size(write_buffer));
                    write_buffer.clear();
                }
            }

            ev.free_read_request(req);
        }

        if (!std::ranges::empty(write_buffer))
        {
            auto lock = std::unique_lock{write_mutex};
            ::write(STDOUT_FILENO, std::ranges::data(write_buffer), std::ranges::size(write_buffer));
        }
    }

    auto operator()(ReadRequest *req) -> std::expected<std::size_t, std::string>
    {
        auto lock = std::unique_lock(mutex);
        to_process.push_back(req);

        cv.notify_one();

        return 1zu;
    }

    inline static const auto hs_database_free = [](::hs_database_t *db) { ::hs_free_database(db); };
    inline static const auto hs_scratch_free = [](::hs_scratch_t *s) { ::hs_free_scratch(s); };

    EV &ev;
    std::unique_ptr<::hs_database_t, decltype(hs_database_free)> db;
    std::unique_ptr<::hs_scratch_t, decltype(hs_scratch_free)> scratch;
    std::deque<ReadRequest *> to_process;
    std::mutex mutex;
    std::mutex write_mutex;
    std::condition_variable cv;
    std::atomic<bool> running;
    std::vector<std::jthread> workers;
};

}
