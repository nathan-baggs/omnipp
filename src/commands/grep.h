#pragma once

#include <cstring>
#include <fcntl.h>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <dirent.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "config.h"
#include "utils/event_loop.h"

namespace om
{

namespace impl
{

struct OpenAtHandler
{
    auto operator()(auto &ev, int fd) const noexcept -> void
    {
        std::println("openat success: {}", fd);
        ev.queue_getdents(fd);
    }
};

struct GetDentsHandler
{
    auto operator()([[maybe_unused]] auto &ev, int parent_fd, bool is_file, ::beman::cstring_view path) const noexcept
        -> void
    {
        std::println("found: {} [is_file: {}]", path, is_file);

        if (!is_file && path != "." && path != "..")
        {
            ev.queue_openat(parent_fd, path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        }
    }
};

struct CloseHandler
{
    auto operator()([[maybe_unused]] auto &ev)
    {
        std::println("close called");
    }
};

}

inline auto grep([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args)
{
    if (std::ranges::size(args) != 2)
    {
        throw std::runtime_error("expected args: [regex, location]");
    }

    [[maybe_unused]] const auto regex = args[0];
    const auto location = args[1];

    auto ev = EventLoop{impl::OpenAtHandler{}, impl::GetDentsHandler{}, impl::CloseHandler{}};

    ev.queue_openat(location, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

    const auto res = ev.pump();
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}
}
