#pragma once

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

enum class Op
{
    OPENAT,
    GETDENTS,
    CLOSE,
};

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
}

template <class OpenAtHandler, class GetDentsHandler, class CloseHandler>
class EventLoop
{
  public:
    EventLoop(OpenAtHandler openat_handler, GetDentsHandler getdents_handler, CloseHandler close_handler)
        : openat_handler_{std::move(openat_handler)}
        , getdents_handler_{std::move(getdents_handler)}
        , close_handler_{std::move(close_handler)}
        , ring_{new ::io_uring{}}
        , in_flight_{}
        , request_pool_(impl::queue_size)
        , request_free_list_{}
        , getdents_queue_{}
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
        auto *req = next_request<impl::Op::OPENAT>(-1, std::string{path.c_str()});

        contract_assert(!!req->path);
        ::io_uring_prep_openat(req->sqe, AT_FDCWD, req->path->c_str(), flags, 0u);
    }

    auto queue_getdents(int fd) noexcept -> void
    {
        auto *req = next_request<impl::Op::GETDENTS>(fd);
        getdents_queue_.push_back(req);
    }

    auto queue_close(int fd) noexcept -> void
    {
        auto *req = next_request<impl::Op::CLOSE>();

        ::io_uring_prep_close(req->sqe, fd);
    }

    [[nodiscard]] auto pump() noexcept -> std::expected<void, std::string>
    {
        while (in_flight_ > 0)
        {
            for (const auto *req : getdents_queue_)
            {
                for (;;)
                {
                    const auto read = ::syscall(
                        SYS_getdents64, req->fd, std::ranges::data(req->buffer), std::ranges::size(req->buffer));

                    contract_assert(read >= 0);

                    if (read == 0)
                    {
                        break;
                    }

                    auto res_span = std::span(std::ranges::data(req->buffer), read);

                    while (!std::ranges::empty(res_span))
                    {
                        const auto *dir = reinterpret_cast<const impl::linux_dirent64 *>(std::ranges::data(res_span));
                        const auto is_file = dir->d_type == DT_REG;
                        const auto is_dir = dir->d_type == DT_DIR;

                        if (is_file || is_dir)
                        {
                            getdents_handler_(*this, is_file, ::beman::cstring_view{dir->d_name});
                        }

                        res_span = res_span.subspan(dir->d_reclen);
                    }
                }
            }

            if (::io_uring_submit_and_wait(ring_.get(), 1u) < 0)
            {
                return std::unexpected("failed to submit pending requests");
            }

            ::io_uring_cqe *cqe = {};
            auto head = unsigned{};
            auto count = unsigned{};

            io_uring_for_each_cqe(ring_.get(), head, cqe)
            {
                auto *req = reinterpret_cast<Request *>(::io_uring_cqe_get_data(cqe));
                contract_assert(req != nullptr);

                const auto res = cqe->res;
                if (res < 0)
                {
                    std::println("{} {}", -res, std::to_underlying(req->op));
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
                        openat_handler_(*this, res);

                        break;
                    }
                    case GETDENTS:
                    {
                        if (res == 0)
                        {
                            queue_close(req->fd);
                        }
                        else
                        {
                        }
                        break;
                    }
                    case CLOSE:
                    {
                        close_handler_(*this);
                        break;
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
        std::vector<std::byte> buffer = std::vector<std::byte>(32zu * 1024zu);
        std::optional<std::string> path = {};
        int fd = -1;
        std::size_t offset = {};
    };

    inline static const auto ring_free = [](auto *r) { ::io_uring_queue_exit(r); };

    template <impl::Op O>
    [[nodiscard]] auto next_request(
        int fd = -1,
        std::optional<std::string> path = std::nullopt,
        std::size_t offset = 0zu) noexcept -> Request * //
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
    std::unique_ptr<::io_uring, decltype(ring_free)> ring_;
    std::size_t in_flight_;
    std::vector<Request> request_pool_;
    std::inplace_vector<Request *, impl::queue_size> request_free_list_;
    std::vector<Request *> getdents_queue_;
};
}
