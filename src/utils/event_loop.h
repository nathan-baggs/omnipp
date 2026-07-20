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
        , request_pool_(impl::queue_size)
        , request_free_list_{}
        , getdents_queue_{}
        , close_queue_(impl::max_fd())
    {
        {
            if (::io_uring_queue_init(impl::queue_size, ring_.get(), 0u) != 0)
            {
                throw std::runtime_error("failed to create uring queue");
            }

            request_free_list_ = request_pool_ | std::views::transform([](auto &e) { return std::addressof(e); }) |
                                 std::ranges::to<std::inplace_vector<Request *, impl::queue_size>>();
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
        auto *req = next_request<impl::Op::OPENAT>(parent_fd, std::string{path.c_str()}, 0zu, !(flags & O_DIRECTORY));
        std::println("{} {} {} {}", *req->path, req->is_file, flags, flags & O_DIRECTORY);

        contract_assert(!!req->path);
        ::io_uring_prep_openat(req->sqe, parent_fd, req->path->c_str(), flags, 0u);

        if (parent_fd != AT_FDCWD)
        {
            ++close_queue_[parent_fd];
        }
    }

    auto queue_getdents(int fd) noexcept -> void
    {
        getdents_queue_.push_back({fd});
        ++in_flight_;
    }

    auto queue_close(int fd) noexcept -> void
    {
        auto *req = next_request<impl::Op::CLOSE>();

        ::io_uring_prep_close(req->sqe, fd);
    }

    auto queue_read(int fd, std::size_t offset = 0zu) noexcept -> void
    {
        auto *req = next_request<impl::Op::READ>(fd, std::nullopt, offset);
        ::io_uring_prep_read(req->sqe, fd, std::ranges::data(req->buffer), std::ranges::size(req->buffer), offset);
    }

    [[nodiscard]] auto pump() noexcept -> std::expected<void, std::string>
    {
        while (in_flight_ > 0)
        {
            auto current_getdents_queue = std::vector<GetDentsRequest>{};
            std::ranges::swap(current_getdents_queue, getdents_queue_);

            for (const auto &req : current_getdents_queue)
            {
                ++close_queue_[req.fd];

                for (;;)
                {
                    const auto read =
                        ::syscall(SYS_getdents64, req.fd, std::ranges::data(req.buffer), std::ranges::size(req.buffer));

                    std::println("fd: {} {}", req.fd, errno);
                    contract_assert(read >= 0);

                    if (read == 0)
                    {
                        break;
                    }

                    auto res_span = std::span(std::ranges::data(req.buffer), read);

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

                if (--close_queue_[req.fd] == 0)
                {
                    queue_close(req.fd);
                }

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
                auto *req = reinterpret_cast<Request *>(::io_uring_cqe_get_data(cqe));
                contract_assert(req != nullptr);

                const auto res = cqe->res;
                if (res < 0)
                {
                    std::println("{} {} {}", -res, std::to_underlying(req->op), *req->path);
                    contract_assert(res >= 0);
                    free_request(req);
                    ++count;
                    continue;
                }

                switch (req->op)
                {
                    using enum impl::Op;

                    case OPENAT:
                    {
                        if (req->fd != AT_FDCWD)
                        {
                            if (--close_queue_[req->fd] == 0)
                            {
                                queue_close(req->fd);
                            }
                        }

                        openat_handler_(*this, res, req->is_file);

                        break;
                    }
                    case GETDENTS:
                    {
                        // handle synchronously
                        break;
                    }
                    case CLOSE:
                    {
                        close_handler_(*this);
                        break;
                    }
                    case READ:
                    {
                        if (res == 0)
                        {
                            queue_close(req->fd);
                        }
                        else
                        {
                            const auto data = std::span(std::ranges::data(req->buffer), res);

                            read_handler_(*this, req->fd, data);
                            queue_read(req->fd, req->offset + res);
                        }
                    }
                }

                free_request(req);
                ++count;
            }

            ::io_uring_cq_advance(ring_.get(), count);
        }

        return {};
    }

  private:
    struct Request
    {
        impl::Op op = {};
        ::io_uring_sqe *sqe = {};
        std::optional<std::string> path = {};
        int fd = -1;
        std::size_t offset{};
        std::vector<std::byte> buffer = std::vector<std::byte>(64zu * 1024zu);
        bool is_file = false;
    };

    struct GetDentsRequest
    {
        int fd = -1;
        std::vector<std::byte> buffer = std::vector<std::byte>(32zu * 1024zu);
    };

    inline static const auto ring_free = [](auto *r) { ::io_uring_queue_exit(r); };

    template <impl::Op O>
    [[nodiscard]] auto next_request(
        int fd = -1,
        std::optional<std::string> path = std::nullopt,
        std::size_t offset = 0zu,
        bool is_file = false) noexcept -> Request * //
        pre(!request_free_list_.empty())
    {
        auto *sqe = ::io_uring_get_sqe(ring_.get());
        contract_assert(sqe != nullptr);

        auto *next = request_free_list_.back();
        request_free_list_.pop_back();

        next->op = O;
        next->sqe = sqe;
        next->path = std::move(path);
        next->fd = fd;
        next->offset = offset;
        next->is_file = is_file;

        io_uring_sqe_set_data(sqe, next);
        ++in_flight_;

        return next;
    }

    auto free_request(Request *req) noexcept -> void //
        pre(std::ranges::size(request_free_list_) != impl::queue_size)
    {
        request_free_list_.unchecked_push_back(req);
        --in_flight_;
    }

    OpenAtHandler openat_handler_;
    GetDentsHandler getdents_handler_;
    CloseHandler close_handler_;
    ReadHandler read_handler_;
    std::unique_ptr<::io_uring, decltype(ring_free)> ring_;
    std::size_t in_flight_;
    std::vector<Request> request_pool_;
    std::inplace_vector<Request *, impl::queue_size> request_free_list_;
    std::vector<GetDentsRequest> getdents_queue_;
    std::vector<std::uint32_t> close_queue_;
};
}
