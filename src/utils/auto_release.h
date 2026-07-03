#pragma once

#include <functional>
#include <ranges>
#include <utility>

namespace om
{

template <class T, class D, T Invalid = {}>
    requires std::is_trivially_copyable_v<T>
class AutoRelease
{
  public:
    constexpr AutoRelease();

    constexpr AutoRelease(T obj);

    constexpr ~AutoRelease();

    AutoRelease(const AutoRelease &) = delete;

    auto operator=(const AutoRelease &) -> AutoRelease & = delete;

    constexpr AutoRelease(AutoRelease &&other);

    constexpr auto operator=(AutoRelease &&other) -> AutoRelease &;

    constexpr auto swap(AutoRelease &other) noexcept -> void;

    constexpr auto reset(T obj) -> void;

    constexpr T get() const;

    constexpr operator T() const;

    constexpr explicit operator bool() const;

    constexpr T *operator&() noexcept;

  private:
    T obj_;
    [[no_unique_address]] D deleter_;
};

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::AutoRelease()
    : AutoRelease(Invalid)
{
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::AutoRelease(T obj)
    : obj_(obj)
    , deleter_{}
{
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::~AutoRelease()
{
    if (obj_ != Invalid)
    {
        deleter_(obj_);
    }
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::AutoRelease(AutoRelease &&other)
    : AutoRelease()
{
    swap(other);
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr auto AutoRelease<T, D, Invalid>::operator=(AutoRelease &&other) -> AutoRelease &
{
    AutoRelease new_obj{std::move(other)};
    swap(new_obj);

    return *this;
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr auto AutoRelease<T, D, Invalid>::swap(AutoRelease &other) noexcept -> void
{
    std::ranges::swap(obj_, other.obj_);
    std::ranges::swap(deleter_, other.deleter_);
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr auto AutoRelease<T, D, Invalid>::reset(T obj) -> void
{
    if (obj_ != Invalid)
    {
        deleter_(obj_);
    }

    obj_ = obj;
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr T AutoRelease<T, D, Invalid>::get() const
{
    return obj_;
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::operator T() const
{
    return obj_;
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr AutoRelease<T, D, Invalid>::operator bool() const
{
    return obj_ != Invalid;
}

template <class T, class D, T Invalid>
    requires std::is_trivially_copyable_v<T>
constexpr T *AutoRelease<T, D, Invalid>::operator&() noexcept
{
    return std::addressof(obj_);
}

}
