#pragma once

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <iostream>
#include <mutex>
#include <print>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <variant>

#include <dirent.h>
#include <fcntl.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "config.h"
#include "pipeline/pipeline.h"
#include "sink/write.h"
#include "source/read_file.h"
#include "transform/vectorscan_regex.h"
#include "utils/event_loop.h"
#include "utils/regex_handler.h"
#include "utils/request_pool.h"

namespace om
{

namespace impl
{

class Processor
{
  public:
    ~Processor()
    {
        running_ = false;
        cv_.notify_all();
    }

    auto run(::beman::cstring_view location, ::beman::cstring_view regex) -> void
    {
        running_ = true;

        const auto start_dir = ::openat(AT_FDCWD, location.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME);
        if (start_dir < 0)
        {
            throw std::runtime_error(std::format("failed to open initial directory: {}", location));
        }

        queue_.push_back(start_dir);
        pending_ = 1zu;

        for (auto i = 0zu; i < std::jthread::hardware_concurrency() - 1zu; ++i)
        {
            std::println("starting thread");
            threads_.emplace_back([&] { worker(regex); });
        }

        worker(regex);
    }

  private:
    auto worker(::beman::cstring_view regex) -> void
    {
        auto pipeline = source::ReadFile{} | transform::VectorScan{regex} | sink::Write{};

        while (running_)
        {
            auto dir_fd = int{-1};

            {
                auto lock = std::unique_lock{mutex_};
                cv_.wait(lock, [this] { return !std::ranges::empty(queue_) || !running_; });

                if (!running_)
                {
                    break;
                }

                dir_fd = queue_.front();
                queue_.pop_front();
            }

            thread_local auto getdent_buffer = std::vector<std::byte>(32zu * 1024zu);

            for (;;)
            {
                const auto read = ::syscall(
                    SYS_getdents64, dir_fd, std::ranges::data(getdent_buffer), std::ranges::size(getdent_buffer));

                if (read < 0)
                {
                    std::println(std::cerr, "fd: {} {}", dir_fd, errno);
                    break;
                }

                if (read == 0)
                {
                    break;
                }

                auto res_span = std::span(std::ranges::data(getdent_buffer), read);

                thread_local auto local_dir_queue = std::vector<int>{};

                while (!std::ranges::empty(res_span))
                {
                    const auto *dir = reinterpret_cast<const impl::linux_dirent64 *>(std::ranges::data(res_span));
                    const auto name = ::beman::cstring_view{dir->d_name};

                    const auto is_file = dir->d_type == DT_REG;
                    const auto is_dir = dir->d_type == DT_DIR;

                    if (is_dir)
                    {
                        if (name == "."sv || name == ".."sv)
                        {
                            res_span = res_span.subspan(dir->d_reclen);
                            continue;
                        }

                        const auto next_dir =
                            ::openat(dir_fd, name.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_NOATIME);
                        if (next_dir < 0)
                        {
                            res_span = res_span.subspan(dir->d_reclen);
                            continue;
                        }

                        local_dir_queue.push_back(next_dir);
                    }
                    else if (is_file)
                    {
                        const auto next_file = ::openat(dir_fd, name.c_str(), O_RDONLY | O_NOFOLLOW | O_NOATIME);
                        if (next_file < 0)
                        {
                            res_span = res_span.subspan(dir->d_reclen);
                            continue;
                        }

                        struct statx stx{};
                        if (::statx(next_file, "", AT_EMPTY_PATH, STATX_SIZE, &stx) != 0)
                        {
                            res_span = res_span.subspan(dir->d_reclen);
                            continue;
                        }

                        execute(pipeline, next_file, stx.stx_size);

                        ::close(next_file);
                    }

                    res_span = res_span.subspan(dir->d_reclen);
                }

                pending_.fetch_add(std::ranges::size(local_dir_queue), std::memory_order_relaxed);

                {
                    auto lock = std::unique_lock{mutex_};
                    queue_.append_range(local_dir_queue);
                    local_dir_queue.clear();
                }

                cv_.notify_one();
            }

            ::close(dir_fd);
            if (pending_.fetch_sub(1zu, std::memory_order_acq_rel) == 1zu)
            {
                running_ = false;
                cv_.notify_all();
            }
        }
    }

    std::deque<int> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> running_;
    std::vector<std::jthread> threads_;
    std::atomic<std::size_t> pending_;
};

}

inline auto grep([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args)
{
    if (std::ranges::size(args) != 2)
    {
        throw std::runtime_error("expected args: [regex, location]");
    }

    const auto regex = args[0];
    const auto location = args[1];

    auto processor = impl::Processor{};
    processor.run(location, regex);
}
}
