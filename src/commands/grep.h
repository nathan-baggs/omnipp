#pragma once

#include <cstring>
#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include <beman/cstring_view/cstring_view.hpp>

#include "pipeline/pipeline.h"
#include "sink/cout.h"
#include "source/directory_traverse_source.h"
#include "source/mapped_file.h"
#include "transform/simple_regex.h"

namespace om
{

inline auto grep([[maybe_unused]] const Config &config, std::span<const ::beman::cstring_view> args) //
{
    if (std::ranges::size(args) != 2)
    {
        throw std::runtime_error("expected args: [regex, location]");
    }

    const auto regex = std::string{args[0].c_str()};
    const auto location = args[1];

    const auto pipeline =
        source::DirectoryTraverse{} | source::MappedFile{} | transform::SimpleRegex{regex} | sink::Cout{};

    execute(pipeline, location);
}

}
