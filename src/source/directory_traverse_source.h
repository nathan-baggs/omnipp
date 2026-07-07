#pragma once

#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <ranges>
#include <string>

#include <fcntl.h>

#include <beman/cstring_view/cstring_view.hpp>

#include "concepts/concepts.h"
#include "utils/auto_release.h"
#include "utils/fd_closer.h"

using namespace std::literals;

namespace om::source
{

namespace impl
{
}

struct DirectoryTraverse
{
    using is_source = bool;

    constexpr static auto max_size = 10zu * 1024zu * 1024zu;

    template <class N>
    [[nodiscard]] auto operator()(beman::cstring_view dir) const noexcept -> std::expected<std::size_t, std::string>
    {
        const auto root = std::filesystem::path{dir.c_str()};
        if (!(std::filesystem::exists(root) && std::filesystem::is_directory(root)))
        {
            return std::unexpected{std::format("{} not a directory", root)};
        }

        auto matches = std::size_t{};

        for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
        {
            auto fd = AutoRelease<int, FdCloser, -1>{::openat(AT_FDCWD, entry.path().c_str(), O_RDONLY)};
            if (!fd)
            {
                return std::unexpected{std::format("failed to open file: {}", entry.path())};
            }

            const auto processed = N{}(fd.get(), std::filesystem::file_size(entry));
            if (!processed)
            {
                return processed.error();
            }

            matches += *processed;
        }

        return matches;
    }
};

static_assert(Source<DirectoryTraverse>);

}
