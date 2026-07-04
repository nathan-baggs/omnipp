#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <vector>

namespace om
{

struct NullSink
{
    using is_sink = bool;
    auto operator()(auto &&...) const -> std::expected<std::size_t, std::string>
    {
        return 0;
    }
};

template <class T>
concept Source = requires(T) {
    { typename T::is_source{} } -> std::same_as<bool>;
};

template <class T>
concept Sink = requires(T) {
    { typename T::is_sink{} } -> std::same_as<bool>;
};

template <class T>
concept Transform = requires(T t, std::span<const std::byte> data) {
    { t.template operator()<NullSink>(data) } -> std::same_as<std::expected<std::size_t, std::string>>;
};

template <class T>
concept Composable = Source<T> || Transform<T> || Sink<T>;
}
