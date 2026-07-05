#pragma once

#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <sys/stat.h>

#include <beman/cstring_view/cstring_view.hpp>
#include <unistd.h>

#include "config/config.h"
#include "pipeline/pipeline.h"
#include "sink/cout.h"
#include "sink/send_file.h"
#include "sink/write.h"
#include "source/mapped_chunk_file.h"
#include "source/mapped_file.h"
#include "source/read_file.h"
#include "source/zero_copy_read.h"
#include "transform/tree_sitter.h"
#include "utils/auto_release.h"

namespace om
{

namespace impl
{

struct File
{
    AutoRelease<int, FdCloser, -1> fd;
    std::size_t size;
};

auto open_file(::beman::cstring_view path) -> File
{
    auto fd = AutoRelease<int, FdCloser, -1>{::openat(AT_FDCWD, path.c_str(), O_RDONLY)};
    if (!fd)
    {
        throw std::runtime_error(std::format("failed to open: {}", path));
    }

    struct statx stx{};
    if (::statx(fd, "", AT_EMPTY_PATH, STATX_SIZE, &stx) != 0)
    {
        throw std::runtime_error(std::format("failed to statx file: {}", ::strerror(errno)));
    }

    return {.fd = std::move(fd), .size = stx.stx_size};
}

template <class... Args>
auto execute(const auto &pipeline, Args &&...args)
{
    const auto res = pipeline(std::forward<Args>(args)...);
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}

auto can_zero_copy() noexcept -> bool
{
    return !::isatty(STDOUT_FILENO);
}

}

inline auto cat(const Config &config, std::span<const ::beman::cstring_view> args) //
    pre(!std::ranges::empty(args))
{
    const auto file = impl::open_file(args[0]);

    if (impl::can_zero_copy())
    {
        const auto pipeline = source::ZeroCopyRead{} | sink::SendFile{};
        impl::execute(pipeline, file.fd.get(), file.size);
        return;
    }

    if (file.size < source::ReadFile::max_size)
    {
        if (config.colour_output)
        {
            const auto pipeline = source::ReadFile{} | transform::TreeSitter{} | sink::Write{};
            impl::execute(pipeline, file.fd.get(), file.size);
        }
        else
        {
            const auto pipeline = source::ReadFile{} | sink::Write{};
            impl::execute(pipeline, file.fd.get(), file.size);
        }
    }
    else
    {
        const auto pipeline = source::ReadFile{} | sink::Write{};
        impl::execute(pipeline, file.fd.get(), file.size);
    }
}

}
