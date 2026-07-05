#pragma once

#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <beman/cstring_view/cstring_view.hpp>

#include "config/config.h"
#include "pipeline/pipeline.h"
#include "sink/stdout.h"
#include "sink/write.h"
#include "source/mapped_file.h"
#include "source/read_file.h"
#include "transform/tree_sitter.h"

namespace om
{

namespace impl
{

auto execute(const auto &pipeline, auto arg)
{
    const auto res = pipeline(arg);
    if (!res)
    {
        throw std::runtime_error(res.error());
    }
}

}

inline auto cat(const Config &config, std::span<const ::beman::cstring_view> args) //
    pre(!std::ranges::empty(args))
{
    if (config.colour_output)
    {
        const auto pipeline = source::ReadFile{} | transform::TreeSitter{} | sink::Stdout{};
        impl::execute(pipeline, args[0]);
    }
    else
    {
        const auto pipeline = source::ReadFile{} | sink::Stdout{};
        impl::execute(pipeline, args[0]);
    }
}

}
