#pragma once

#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <beman/cstring_view/cstring_view.hpp>

#include "sink/stdout_sink.h"
#include "source/mapped_file_source.h"
#include "transform/tree_sitter_transform.h"

namespace om
{

inline auto cat(std::span<const ::beman::cstring_view> args)
{
    std::println("cat");
    auto source = source::MappedFile{args[0]};

    const auto file = source.read();
    if (!file)
    {
        throw std::runtime_error(file.error());
    }

    const auto transform = transform::TreeSitter{};

    const auto out_data = transform.transform(*file);
    if (!out_data)
    {
        throw std::runtime_error(out_data.error());
    }

    auto sink = sink::Stdout{};

    const auto written_bytes = sink.write(*out_data);
    if (!written_bytes)
    {
        throw std::runtime_error(written_bytes.error());
    }
}

}
