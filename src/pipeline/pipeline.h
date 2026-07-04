#pragma once

#include "concepts/concepts.h"

namespace om
{

namespace impl
{
template <Composable L, Composable R>
struct Composed
{
    using is_sink = bool;

    L left;
    R right;

    template <class... Args>
    constexpr auto operator()(Args &&...args) const noexcept -> std::expected<std::size_t, std::string>
    {
        return left.template operator()<R>(std::forward<Args>(args)...);
    }
};

}

template <Composable L, Composable R>
constexpr auto operator|(L left, R right)
{
    return impl::Composed{std::move(left), std::move(right)};
}

template <class L1, class L2, Composable R>
constexpr auto operator|(impl::Composed<L1, L2> left, R right)
{
    return std::move(left.left) | (std::move(left.right) | std::move(right));
}

}
