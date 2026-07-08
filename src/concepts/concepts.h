#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace om
{

namespace impl
{

struct EmptyState
{
};

struct State
{
};

struct Source
{
};

struct Transform
{
};

struct Sink
{
};

template <class T, template <class> class Node, class State>
struct BaseNode : T, State
{
    template <class... Args>
    BaseNode(Args &&...args)
        : State{std::forward<Args>(args)...}
    {
    }

    template <class Next>
    constexpr auto operator()(Next next) const
    {
        if constexpr (std::same_as<State, EmptyState>)
        {
            return Node{std::move(next)};
        }
        else
        {
            return Node{std::move(next), std::move(*this)};
        }
    }

    constexpr auto operator()() const
        requires std::same_as<T, Sink>
    {
        // bit of a kludge here, actual sink types have to be templated to fit in with the template teplate argument
        // machinery we have - so just pass any type here
        return Node<bool>{};
    }
};

}

template <template <class> class Node, class State = impl::EmptyState>
struct BaseSource : impl::BaseNode<impl::Source, Node, State>
{
};

template <template <class> class Node, class State = impl::EmptyState>
struct BaseTransform : impl::BaseNode<impl::Transform, Node, State>
{
};

template <template <class> class Node, class State = impl::EmptyState>
struct BaseSink : impl::BaseNode<impl::Sink, Node, State>
{
};

template <class T>
concept IsSource = std::is_base_of_v<impl::Source, T>;

template <class T>
concept IsTransform = std::is_base_of_v<impl::Transform, T>;

template <class T>
concept IsSink = std::is_base_of_v<impl::Sink, T>;

template <class T>
concept Composable = IsSource<T> || IsTransform<T> || IsSink<T>;

}
