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

template <class Next>
struct DirectoryTraverseNode
{
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
            if (!std::filesystem::is_regular_file(entry))
            {
                continue;
            }

            auto fd = AutoRelease<int, FdCloser, -1>{::openat(AT_FDCWD, entry.path().c_str(), O_RDONLY)};
            if (!fd)
            {
                return std::unexpected{std::format("failed to open file: {}", entry.path())};
            }

            const auto processed = next(fd.get(), std::filesystem::file_size(entry));
            if (!processed)
            {
                return std::unexpected{processed.error()};
            }

            matches += *processed;
        }

        return matches;
    }

    Next next;
};

struct DirectoryTraverse : BaseSource<DirectoryTraverseNode>
{
};

static_assert(IsSource<DirectoryTraverse>);

}
