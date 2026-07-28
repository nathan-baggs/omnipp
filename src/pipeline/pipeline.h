#pragma once

#include <stdexcept>

#include "concepts/concepts.h"

namespace om
{

namespace impl
{
template <Composable L, Composable R>
struct Composed : impl::Source, impl::Transform // a bit gross but ok for now
{
    template <class Next>
    constexpr auto operator()(Next next)
    {
        return left(right(std::move(next)));
    }

    L left;
    R right;
};

}

template <Composable L, Composable R>
constexpr auto operator|(L left, R right)
{
    if constexpr (IsSink<R>)
    {
        return left(right());
    }
    else
    {
        return impl::Composed{.left = std::move(left), .right = std::move(right)};
    }
}

template <std::size_t Index, std::size_t Counter = 0zu>
auto &get(auto &node)
{
    if constexpr (Index == Counter)
    {
        return node;
    }
    else
    {
        return get<Index, Counter + 1zu>(node.next);
    }
}

template <class... Args>
auto execute(auto &pipeline, Args &&...args)
{
    const auto res = pipeline(std::forward<Args>(args)...);
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}

}
