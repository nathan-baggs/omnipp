#pragma once

#include <deque>
#include <inplace_vector>
#include <ranges>
#include <type_traits>
#include <vector>

#include <liburing.h>

namespace om
{

enum class Op
{
    OPENAT,
    CLOSE,
    READ,
    WRITE,
};

struct BaseRequest
{
    Op op;
    ::io_uring_sqe *sqe{};
};

struct OpenAtRequest : BaseRequest
{
    auto reset(int parent_fd, std::string path, int flags) -> void
    {
        this->path = std::move(path);
        this->parent_fd = parent_fd;
        this->flags = flags;
        this->is_file = !(flags & O_DIRECTORY);
    }

    auto prep() -> void
    {
        ::io_uring_prep_openat(sqe, parent_fd, path.c_str(), flags, 0u);
    }

    std::string path;
    int fd = -1;
    int flags = 0;
    bool is_file = false;
    int parent_fd = -1;
};

struct CloseRequest : BaseRequest
{
    auto reset(int fd) -> void
    {
        this->fd = fd;
    }

    auto prep() -> void
    {
        ::io_uring_prep_close(sqe, fd);
    }

    int fd = -1;
};

struct ReadRequest : BaseRequest
{
    auto reset(int fd, std::size_t offset) -> void
    {
        this->fd = fd;
        this->offset = offset;
    }

    auto prep() -> void
    {
        ::io_uring_prep_read(sqe, fd, std::ranges::data(buffer), std::ranges::size(buffer), offset);
    }

    int fd = -1;
    std::size_t offset{};
    std::vector<std::byte> buffer = std::vector<std::byte>(64zu * 1024zu);
};

struct WriteRequest : BaseRequest
{
    auto reset(int fd, std::string buffer, std::size_t offset) -> void
    {
        this->fd = fd;
        this->offset = offset;
        this->buffer = std::move(buffer);
    }

    auto prep() -> void
    {
        ::io_uring_prep_write(sqe, fd, std::ranges::data(buffer) + offset, std::ranges::size(buffer), -1);
    }

    int fd = -1;
    std::size_t offset{};
    std::string buffer;
};

template <class T>
concept IsRequest = std::is_base_of_v<BaseRequest, T>;

template <Op O, IsRequest T, std::size_t N = 4096zu>
class RequestPool
{
  public:
    constexpr RequestPool(::io_uring *ring, std::size_t &in_flight) //
        pre(ring != nullptr);

    constexpr RequestPool(const RequestPool &) = delete;
    constexpr auto operator=(const RequestPool &) -> RequestPool & = delete;
    constexpr RequestPool(RequestPool &&) = default;
    constexpr auto operator=(RequestPool &&) -> RequestPool & = delete;

    auto try_pop_overflow() -> bool;

    template <class... Args>
    auto next(Args &&...args) -> void;

    auto free(T *req) noexcept -> void;

  private:
    ::io_uring *ring_;
    std::vector<T> pool_;
    std::inplace_vector<T *, N> free_list_;
    std::size_t &in_flight_;
    std::deque<T *> sqe_overflow_;
    std::deque<T> pool_overflow_;
    std::vector<T *> pool_overflow_free_list_;
};

template <Op O, IsRequest T, std::size_t N>
constexpr RequestPool<O, T, N>::RequestPool(::io_uring *ring, std::size_t &in_flight)
    : ring_{ring}
    , pool_{N}
    , free_list_{}
    , in_flight_{in_flight}
    , sqe_overflow_{}
    , pool_overflow_{}
    , pool_overflow_free_list_{}
{
    free_list_ = pool_ |
                 std::views::transform(
                     [](auto &e)
                     {
                         e.op = O;
                         return std::addressof(e);
                     }) |
                 std::ranges::to<std::inplace_vector<T *, N>>();

    pool_overflow_free_list_.reserve(N);
}

template <Op O, IsRequest T, std::size_t N>
auto RequestPool<O, T, N>::try_pop_overflow() -> bool
{
    if (std::ranges::empty(sqe_overflow_))
    {
        return false;
    }

    auto *sqe = ::io_uring_get_sqe(ring_);
    if (sqe == nullptr)
    {
        return false;
    }

    auto *next = sqe_overflow_.front();
    sqe_overflow_.pop_front();
    next->sqe = sqe;
    next->prep();

    ::io_uring_sqe_set_data(sqe, next);
    ++in_flight_;

    return true;
}

template <Op O, IsRequest T, std::size_t N>
template <class... Args>
auto RequestPool<O, T, N>::next(Args &&...args) -> void
{
    T *next = nullptr;

    if (!std::ranges::empty(free_list_))
    {
        next = free_list_.back();
        free_list_.pop_back();
    }
    else if (!std::ranges::empty(pool_overflow_free_list_))
    {
        next = pool_overflow_free_list_.back();
        pool_overflow_free_list_.pop_back();
    }
    else
    {
        pool_overflow_.push_back({});
        auto &req = pool_overflow_.back();
        req.op = O;
        next = std::addressof(req);
    }

    next->reset(std::forward<Args>(args)...);

    auto *sqe = ::io_uring_get_sqe(ring_);
    if (sqe == nullptr)
    {
        sqe_overflow_.push_back(next);
    }
    else
    {
        next->sqe = sqe;
        next->prep();

        ::io_uring_sqe_set_data(sqe, next);
        ++in_flight_;
    }
}

template <Op O, IsRequest T, std::size_t N>
auto RequestPool<O, T, N>::free(T *req) noexcept -> void
{
    const auto *pool_start = std::ranges::data(pool_);
    const auto *pool_end = pool_start + N;

    if (req >= pool_start && req < pool_end)
    {
        free_list_.unchecked_push_back(req);
    }
    else
    {
        pool_overflow_free_list_.push_back(req);
    }

    --in_flight_;
}

}
