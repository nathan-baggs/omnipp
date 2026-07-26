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
#include "pipeline/pipeline.h"
#include "sink/write.h"
#include "transform/vectorscan_regex.h"
#include "utils/event_loop.h"
#include "utils/regex_handler.h"

namespace om
{

namespace impl
{

struct OpenAtHandler
{
    auto operator()(auto &ev, int fd, bool is_file) const noexcept -> void
    {
        if (is_file)
        {
            ev.queue_read(fd);
        }
        else
        {
            ev.queue_getdents(fd);
        }
    }
};

struct GetDentsHandler
{
    auto operator()(auto &ev, int parent_fd, bool is_file, ::beman::cstring_view path) const noexcept -> void
    {
        if (path != "." && path != "..")
        {
            if (is_file)
            {
                ev.queue_openat(parent_fd, path, O_RDONLY | O_NOFOLLOW);
            }
            else
            {
                ev.queue_openat(parent_fd, path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
            }
        }
    }
};

struct CloseHandler
{
    auto operator()(auto &)
    {
    }
};

struct ReadHandler
{
    template <class EV>
    auto operator()(EV &ev, int, std::vector<std::byte> data) -> std::expected<void, std::string>
    {
        static auto regex_handler = RegexHandler<EV>{ev, regex};

        const auto res = regex_handler(std::move(data));
        if (!res)
        {
            return std::unexpected(res.error());
        }

        return {};
    }

    ::beman::cstring_view regex;
};

struct WriteHandler
{
    auto operator()(auto &, int, std::size_t)
    {
    }
};

}

inline auto grep([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args)
{
    if (std::ranges::size(args) != 2)
    {
        throw std::runtime_error("expected args: [regex, location]");
    }

    const auto regex = args[0];
    const auto location = args[1];

    auto ev = EventLoop{
        impl::OpenAtHandler{},
        impl::GetDentsHandler{},
        impl::CloseHandler{},
        impl::ReadHandler{regex},
        impl::WriteHandler{}};

    ev.queue_openat(location, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

    const auto res = ev.pump();
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}
}
