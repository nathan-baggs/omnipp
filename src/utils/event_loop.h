#pragma once

#include <cerrno>
#include <contracts>
#include <cstddef>
#include <expected>
#include <inplace_vector>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>
#include <liburing.h>

#include "utils/request_pool.h"

// great...
#ifndef IORING_OP_GETDENTS
#define IORING_OP_GETDENTS 50
#endif

namespace om
{

namespace impl
{
static constexpr auto queue_size = 4096zu;
static constexpr auto safe_min = 524'288zu;

extern "C"
{
struct linux_dirent64
{
    std::uint64_t d_ino;
    std::int64_t d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[];
};
}

inline auto max_fd() -> std::size_t
{
    auto rlim = ::rlimit{};
    if (::getrlimit(RLIMIT_NOFILE, &rlim) != 0)
    {
        return safe_min;
    }

    return std::min(static_cast<std::size_t>(rlim.rlim_cur), safe_min);
}

}

template <class OpenAtHandler, class GetDentsHandler, class CloseHandler, class ReadHandler>
class EventLoop
{
  public:
    EventLoop(
        OpenAtHandler openat_handler,
        GetDentsHandler getdents_handler,
        CloseHandler close_handler,
        ReadHandler read_handler)
        : openat_handler_{std::move(openat_handler)}
        , getdents_handler_{std::move(getdents_handler)}
        , close_handler_{std::move(close_handler)}
        , read_handler_{std::move(read_handler)}
        , ring_{new ::io_uring{}}
        , in_flight_{}
        , openat_request_pool_{ring_.get(), in_flight_}
        , close_request_pool_{ring_.get(), in_flight_}
        , read_request_pool_{ring_.get(), in_flight_}
        , getdents_queue_{}
    {
        if (::io_uring_queue_init(impl::queue_size, ring_.get(), 0) != 0)
        {
            throw std::runtime_error("failed to initialise queue");
        }

        getdents_queue_.reserve(100zu);
    }

    ~EventLoop() = default;
    EventLoop(const EventLoop &) = delete;
    auto operator=(const EventLoop &) -> EventLoop & = delete;
    EventLoop(EventLoop &&) = default;
    auto operator=(EventLoop &&) -> EventLoop & = default;

    auto queue_openat(::beman::cstring_view path, int flags) noexcept -> void
    {
        queue_openat(AT_FDCWD, path, flags);
    }

    auto queue_openat(int parent_fd, ::beman::cstring_view path, int flags) noexcept -> void //
        pre(parent_fd == AT_FDCWD || static_cast<std::size_t>(parent_fd) < impl::safe_min)
    {
        openat_request_pool_.next(parent_fd, path.c_str(), flags);
    }

    auto queue_getdents(int fd) noexcept -> void
    {
        getdents_queue_.push_back({fd});
        ++in_flight_;
    }

    auto queue_close(int fd) noexcept -> void
    {
        close_request_pool_.next(fd);
    }

    auto queue_read(int fd, std::size_t offset = 0zu) noexcept -> void
    {
        read_request_pool_.next(fd, offset);
    }

    [[nodiscard]] auto pump() noexcept -> std::expected<void, std::string>
    {
        while (in_flight_ > 0)
        {
            auto current_getdents_queue = std::vector<GetDentsRequest>{};
            std::ranges::swap(current_getdents_queue, getdents_queue_);

            static auto getdent_buffer = std::vector<std::byte>(32zu * 1024zu);

            for (const auto &req : current_getdents_queue)
            {
                for (;;)
                {
                    const auto read = ::syscall(
                        SYS_getdents64, req.fd, std::ranges::data(getdent_buffer), std::ranges::size(getdent_buffer));

                    if (read < 0)
                    {
                        std::println("fd: {} {}", req.fd, errno);
                        break;
                    }

                    if (read == 0)
                    {
                        break;
                    }

                    auto res_span = std::span(std::ranges::data(getdent_buffer), read);

                    while (!std::ranges::empty(res_span))
                    {
                        const auto *dir = reinterpret_cast<const impl::linux_dirent64 *>(std::ranges::data(res_span));
                        const auto is_file = dir->d_type == DT_REG;
                        const auto is_dir = dir->d_type == DT_DIR;

                        if (is_file || is_dir)
                        {
                            getdents_handler_(*this, req.fd, is_file, ::beman::cstring_view{dir->d_name});
                        }

                        res_span = res_span.subspan(dir->d_reclen);
                    }
                }

                queue_close(req.fd);
                --in_flight_;
            }

            ::io_uring_cqe *cqe = {};
            auto ts = ::__kernel_timespec{.tv_sec = 5, .tv_nsec = 0};

            const auto res = ::io_uring_submit_and_wait_timeout(ring_.get(), &cqe, 1u, &ts, nullptr);
            if (res < 0)
            {
                const auto reason = res == -ETIME ? std::format("timeout") : std::format("{}", -res);
                return std::unexpected(std::format("failed to submit pending requests: {}", reason));
            }

            auto head = unsigned{};
            auto count = unsigned{};

            io_uring_for_each_cqe(ring_.get(), head, cqe)
            {
                auto *base_req = reinterpret_cast<BaseRequest *>(::io_uring_cqe_get_data(cqe));
                contract_assert(base_req != nullptr);

                const auto res = cqe->res;
                if (res < 0)
                {
                    std::println("{} {}", -res, std::to_underlying(base_req->op));
                }

                switch (base_req->op)
                {
                    using enum Op;

                    case OPENAT:
                    {
                        auto *req = static_cast<OpenAtRequest *>(base_req);

                        if (res >= 0)
                        {
                            openat_handler_(*this, res, req->is_file);
                        }

                        openat_request_pool_.free(req);
                        break;
                    }
                    case CLOSE:
                    {
                        auto *req = static_cast<CloseRequest *>(base_req);
                        close_handler_(*this);
                        close_request_pool_.free(req);
                        break;
                    }
                    case READ:
                    {
                        auto *req = static_cast<ReadRequest *>(base_req);

                        if (res == 0)
                        {
                            queue_close(req->fd);
                        }
                        else if (res > 0)
                        {
                            const auto data = std::span(std::ranges::data(req->buffer), res);

                            read_handler_(*this, req->fd, data);
                            queue_read(req->fd, req->offset + res);
                        }

                        read_request_pool_.free(req);
                        break;
                    }
                }

                ++count;
            }

            ::io_uring_cq_advance(ring_.get(), count);
        }

        return {};
    }

  private:
    struct GetDentsRequest
    {
        int fd = -1;
    };

    inline static const auto ring_free = [](auto *r)
    {
        ::io_uring_queue_exit(r);
        delete r;
    };

    OpenAtHandler openat_handler_;
    GetDentsHandler getdents_handler_;
    CloseHandler close_handler_;
    ReadHandler read_handler_;
    std::unique_ptr<::io_uring, decltype(ring_free)> ring_;
    std::size_t in_flight_;
    RequestPool<Op::OPENAT, OpenAtRequest, impl::queue_size> openat_request_pool_;
    RequestPool<Op::CLOSE, CloseRequest, impl::queue_size> close_request_pool_;
    RequestPool<Op::READ, ReadRequest, impl::queue_size> read_request_pool_;
    std::vector<GetDentsRequest> getdents_queue_;
};
}
