#pragma once

#include <print>
#include <span>
#include <stdexcept>
#include <string_view>

#include "sink/stdout_sink.h"
#include "source/mapped_file_source.h"

namespace om
{

inline auto cat(std::span<const ::beman::cstring_view> args)
{
    std::println("cat");
    auto source = MappedFileSource{args[0]};

    const auto file = source.read();
    if (!file)
    {
        throw std::runtime_error(file.error());
    }

    auto sink = StdoutSink{};

    const auto written_bytes = sink.write(*file);
    if (!written_bytes)
    {
        throw std::runtime_error(written_bytes.error());
    }
}

}
