#pragma once

#include "concepts/concepts.h"

namespace om
{

namespace impl
{
template <Composable L, Composable R>
struct Composed : impl::Source, impl::Transform // a bit gross but ok for now
{
    template <class Next>
    constexpr auto operator()(Next next) const
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

}
