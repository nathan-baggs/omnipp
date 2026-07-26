#pragma once

#include <cerrno>
#include <contracts>
#include <cstddef>
#include <expected>
#include <inplace_vector>
#include <memory>
#include <mutex>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
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

template <class OpenAtHandler, class GetDentsHandler, class CloseHandler, class ReadHandler, class WriteHandler>
class EventLoop
{
  public:
    EventLoop(
        OpenAtHandler openat_handler,
        GetDentsHandler getdents_handler,
        CloseHandler close_handler,
        ReadHandler read_handler,
        WriteHandler write_handler)
        : openat_handler_{std::move(openat_handler)}
        , getdents_handler_{std::move(getdents_handler)}
        , close_handler_{std::move(close_handler)}
        , read_handler_{std::move(read_handler)}
        , write_handler_{std::move(write_handler)}
        , ring_{new ::io_uring{}}
        , in_flight_{}
        , openat_request_pool_{ring_.get(), in_flight_}
        , close_request_pool_{ring_.get(), in_flight_}
        , read_request_pool_{ring_.get(), in_flight_}
        , write_request_pool_{ring_.get(), in_flight_}
        , getdents_queue_{}
    {
        if (::io_uring_queue_init(
                impl::queue_size, ring_.get(), IORING_SETUP_COOP_TASKRUN | IORING_SETUP_SINGLE_ISSUER) != 0)
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
        if (parent_fd != AT_FDCWD)
        {
            increase_dir_ref(parent_fd);
        }

        openat_request_pool_.next(parent_fd, path.c_str(), flags);
    }

    auto queue_getdents(int fd) noexcept -> void
    {
        getdents_queue_.push_back({fd});
    }

    auto queue_close(int fd) noexcept -> void
    {
        close_request_pool_.next(fd);
    }

    auto queue_read(int fd, std::size_t offset = 0zu) noexcept -> void
    {
        read_request_pool_.next(fd, offset);
    }

    auto queue_write(int fd, std::string buffer, std::size_t offset = 0zu) noexcept -> void
    {
        write_request_pool_.next(fd, std::move(buffer), offset);
    }

    auto free_read_request(ReadRequest *req) -> void
    {
        auto lock = std::unique_lock(read_free_queue_mutex_);
        read_free_queue_.push_back(req);
    }

    [[nodiscard]] auto pump() noexcept -> std::expected<void, std::string>
    {
        for (;;)
        {
            auto current_getdents_queue = std::vector<GetDentsRequest>{};
            std::ranges::swap(current_getdents_queue, getdents_queue_);

            static auto getdent_buffer = std::vector<std::byte>(32zu * 1024zu);

            for (const auto &req : current_getdents_queue)
            {
                dir_ref_count_[req.fd] = 1zu;

                for (;;)
                {
                    const auto read = ::syscall(
                        SYS_getdents64, req.fd, std::ranges::data(getdent_buffer), std::ranges::size(getdent_buffer));

                    if (read < 0)
                    {
                        std::println(std::cerr, "fd: {} {}", req.fd, errno);
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

                decrease_dir_ref(req.fd);
            }

            auto check_overflow = false;
            do
            {
                check_overflow = openat_request_pool_.try_pop_overflow() || close_request_pool_.try_pop_overflow() ||
                                 read_request_pool_.try_pop_overflow();

            } while (check_overflow);

            if (in_flight_ == 0)
            {
                break;
            }

            auto read_free_queue = std::vector<ReadRequest *>{};

            {
                auto lock = std::unique_lock(read_free_queue_mutex_);
                std::ranges::swap(read_free_queue, read_free_queue_);
            }

            for (auto *req : read_free_queue)
            {
                read_request_pool_.free(req);
            }

            const auto res = ::io_uring_submit(ring_.get());
            if (res < 0)
            {
                return std::unexpected(std::format("failed to submit pending requests: {}", -res));
            }

            ::io_uring_cqe *cqe{};
            auto head = unsigned{};
            auto count = unsigned{};

            if (::io_uring_peek_cqe(ring_.get(), &cqe) != 0)
            {
                ::io_uring_wait_cqe(ring_.get(), &cqe);
            }

            io_uring_for_each_cqe(ring_.get(), head, cqe)
            {
                auto *base_req = reinterpret_cast<BaseRequest *>(::io_uring_cqe_get_data(cqe));
                contract_assert(base_req != nullptr);

                const auto res = cqe->res;
                if (res < 0)
                {
                    std::println(std::cerr, "{} {}", -res, std::to_underlying(base_req->op));
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

                        if (req->parent_fd != AT_FDCWD)
                        {
                            decrease_dir_ref(req->parent_fd);
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
                            free_read_request(req);
                            queue_close(req->fd);
                        }
                        else if (res > 0)
                        {
                            if (const auto r = read_handler_(*this, req); !r)
                            {
                                return std::unexpected(r.error());
                            }

                            queue_read(req->fd, req->offset + res);
                        }

                        break;
                    }
                    case WRITE:
                    {
                        auto *req = static_cast<WriteRequest *>(base_req);

                        write_handler_(*this, req->fd, res);

                        if (req->offset + res != std::ranges::size(req->buffer))
                        {
                            queue_write(req->fd, std::move(req->buffer), req->offset + res);
                        }

                        write_request_pool_.free(req);
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

    auto increase_dir_ref(int fd) -> void
    {
        auto entry = dir_ref_count_.find(fd);
        contract_assert(entry != std::ranges::cend(dir_ref_count_));

        ++entry->second;
    }

    auto decrease_dir_ref(int fd) -> void
    {
        auto entry = dir_ref_count_.find(fd);
        contract_assert(entry != std::ranges::cend(dir_ref_count_));
        contract_assert(entry->second != 0zu);

        --entry->second;

        if (entry->second == 0zu)
        {
            queue_close(entry->first);
            dir_ref_count_.erase(entry);
        }
    }

    OpenAtHandler openat_handler_;
    GetDentsHandler getdents_handler_;
    CloseHandler close_handler_;
    ReadHandler read_handler_;
    WriteHandler write_handler_;
    std::unique_ptr<::io_uring, decltype(ring_free)> ring_;
    std::size_t in_flight_;
    RequestPool<Op::OPENAT, OpenAtRequest, impl::queue_size> openat_request_pool_;
    RequestPool<Op::CLOSE, CloseRequest, impl::queue_size> close_request_pool_;
    RequestPool<Op::READ, ReadRequest, impl::queue_size> read_request_pool_;
    RequestPool<Op::WRITE, WriteRequest, impl::queue_size> write_request_pool_;
    std::vector<GetDentsRequest> getdents_queue_;
    std::unordered_map<int, std::size_t> dir_ref_count_;
    std::vector<ReadRequest *> read_free_queue_;
    std::mutex read_free_queue_mutex_;
};
}
