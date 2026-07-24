#pragma once

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
};

struct BaseRequest
{
    Op op;
    ::io_uring_sqe *sqe{};
};

struct OpenAtRequest : BaseRequest
{
    auto reset(int fd, std::string path, int flags) -> void
    {
        this->path = std::move(path);
        this->fd = fd;
        this->flags = flags;
        this->is_file = !(flags & O_DIRECTORY);
    }

    auto prep() -> void
    {
        ::io_uring_prep_openat(sqe, fd, path.c_str(), flags, 0u);
    }

    std::string path;
    int fd = -1;
    int flags = 0;
    bool is_file = false;
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

    constexpr auto empty() const -> bool;

    template <class... Args>
    auto next(Args &&...args) -> T *;

    auto free(T *req) noexcept -> void //
        pre(std::ranges::size(free_list_) != N);

  private:
    ::io_uring *ring_;
    std::vector<T> pool_;
    std::inplace_vector<T *, N> free_list_;
    std::size_t &in_flight_;
};

template <Op O, IsRequest T, std::size_t N>
constexpr RequestPool<O, T, N>::RequestPool(::io_uring *ring, std::size_t &in_flight)
    : ring_{ring}
    , pool_{N}
    , free_list_{}
    , in_flight_{in_flight}
{
    free_list_ = pool_ | std::views::transform([](auto &e) { return std::addressof(e); }) |
                 std::ranges::to<std::inplace_vector<T *, N>>();
}

template <Op O, IsRequest T, std::size_t N>
constexpr auto RequestPool<O, T, N>::empty() const -> bool
{
    return free_list_.empty();
}

template <Op O, IsRequest T, std::size_t N>
template <class... Args>
auto RequestPool<O, T, N>::next(Args &&...args) -> T *
{
    // this should be a pre-condition but it crashes gcc when you have an empty variadic with a pre-condition
    contract_assert(!empty());

    auto *sqe = ::io_uring_get_sqe(ring_);
    contract_assert(sqe != nullptr);

    auto *next = free_list_.back();
    free_list_.pop_back();

    next->op = O;
    next->sqe = sqe;

    next->reset(std::forward<Args>(args)...);
    next->prep();

    ::io_uring_sqe_set_data(sqe, next);
    ++in_flight_;

    return next;
}

template <Op O, IsRequest T, std::size_t N>
auto RequestPool<O, T, N>::free(T *req) noexcept -> void
{
    free_list_.unchecked_push_back(req);
    --in_flight_;
}

}
