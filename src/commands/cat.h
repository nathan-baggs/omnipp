#pragma once

#include <cstring>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "concepts/concepts.h"
#include "config/config.h"
#include "pipeline/pipeline.h"
// #include "sink/Splice.h"
// #include "sink/cout.h"
// #include "sink/send_file.h"
#include "sink/write.h"
// #include "source/DirectSplice.h"
// #include "source/Splice.h"
// #include "source/copy_file_range.h"
// #include "source/mapped_chunk_file.h"
// #include "source/mapped_file.h"
#include "source/read_file.h"
// #include "source/zero_copy_read.h"
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
    int mode;
};

auto open_file(::beman::cstring_view path) -> File
{
    auto fd = AutoRelease<int, FdCloser, -1>{::openat(AT_FDCWD, path.c_str(), O_RDONLY | O_NOATIME)};
    if (!fd)
    {
        throw std::runtime_error(std::format("failed to open: {}", path));
    }

    struct statx stx{};
    if (::statx(fd, "", AT_EMPTY_PATH, STATX_SIZE | STATX_MODE, &stx) != 0)
    {
        throw std::runtime_error(std::format("failed to statx file: {}", ::strerror(errno)));
    }

    if (::posix_fadvise(fd, 0, stx.stx_size, POSIX_FADV_SEQUENTIAL | POSIX_FADV_WILLNEED) != 0)
    {
        throw std::runtime_error(std::format("failed to advise: {}", ::strerror(errno)));
    }

    return {.fd = std::move(fd), .size = stx.stx_size, .mode = stx.stx_mode};
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

auto can_copy_file_range(int in_mode, int out_mode) noexcept -> bool
{
    return S_ISREG(in_mode) && S_ISREG(out_mode);
}

auto can_middleman_splice(int out_mode) noexcept -> bool
{
    return S_ISCHR(out_mode) || S_ISREG(out_mode);
}

auto can_direct_pipe(int out_mode) noexcept -> bool
{
    return S_ISFIFO(out_mode);
}

auto is_terminal_output() noexcept -> bool
{
    return ::isatty(STDOUT_FILENO) == 1;
}

}

inline auto cat([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args) //
    pre(!std::ranges::empty(args))
{
    const auto file = impl::open_file(args[0]);

    struct statx out_stx{};
    if (::statx(STDOUT_FILENO, "", AT_EMPTY_PATH, STATX_MODE, &out_stx) != 0)
    {
        throw std::runtime_error("failed to statx stdout");
    }

    // if (impl::can_copy_file_range(file.mode, out_stx.stx_mode))
    // {
    //     const auto pipeline = source::CopyFileRange{} | NullSink{};
    //     impl::execute(pipeline, file.fd.get(), file.size);
    //     return;
    // }
    // else if (impl::can_direct_pipe(out_stx.stx_mode))
    // {
    //     const auto pipeline = source::DirectSplice{} | NullSink{};
    //     impl::execute(pipeline, file.fd.get(), file.size);
    //     return;
    // }
    // else if (impl::can_middleman_splice(out_stx.stx_mode))
    // {
    //     const auto pipeline = source::Splice{} | sink::Splice{};
    //     impl::execute(pipeline, file.fd.get(), file.size);
    //     return;
    // }
    // else
    // {
    //     if (config.colour_output || impl::is_terminal_output())
    //     {
    //         const auto pipeline = source::ReadFile{} | transform::TreeSitter{} | sink::Write{};
    //         impl::execute(pipeline, file.fd.get(), file.size);
    //         return;
    //     }
    //     else
    //     {
    //         const auto pipeline = source::ReadFile{} | sink::Write{};
    //         impl::execute(pipeline, file.fd.get(), file.size);
    //         return;
    //     }
    // }

    const auto pipeline = source::ReadFile{} | transform::TreeSitter{} | sink::Write{};
    impl::execute(pipeline, file.fd.get(), file.size);
}

}
