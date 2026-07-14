#pragma once

#include "concepts/concepts.h"

namespace om::sink
{
template <class T>
struct NullSinkNode
{
    auto operator()(auto &&...) const -> std::expected<std::size_t, std::string>
    {
        return 0;
    }
};

struct NullSink : BaseSink<NullSinkNode>
{
};

static_assert(IsSink<NullSink>);

}
