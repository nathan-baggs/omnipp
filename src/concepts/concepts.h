#pragma once

#include <concepts>
#include <cstddef>
#include <expected>
#include <span>
#include <string>

namespace om
{

template <class T>
concept Source = requires(T t) {
    { t.read() } -> std::same_as<std::expected<std::span<const std::byte>, std::string>>;
};

template <class T>
concept Sink = requires(T t, std::span<const std::byte> data) {
    { t.write(data) } -> std::same_as<std::expected<std::size_t, std::string>>;
};

}
