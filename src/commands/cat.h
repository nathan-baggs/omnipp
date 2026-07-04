#pragma once

#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <beman/cstring_view/cstring_view.hpp>

#include "pipeline/pipeline.h"
#include "sink/stdout_sink.h"
#include "source/mapped_file_source.h"
#include "transform/tree_sitter_transform.h"

namespace om
{

inline auto cat(std::span<const ::beman::cstring_view> args) //
    pre(!std::ranges::empty(args))
{
    const auto pipeline = source::MappedFile{} | transform::TreeSitter{} | sink::Stdout{};

    const auto res = pipeline(args[0]);
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}

}
