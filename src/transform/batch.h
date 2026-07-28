#pragma once

#include <cstddef>
#include <exception>
#include <expected>
#include <memory>
#include <print>
#include <span>
#include <string>

#include <beman/cstring_view/cstring_view.hpp>
#include <hs.h>
#include <vector>

#include "concepts/concepts.h"
#include "hs_common.h"
#include "hs_compile.h"
#include "hs_runtime.h"

using namespace std::literals;

namespace om::transform
{

namespace impl
{
static constexpr auto buffer_size = 64zu * 1024zu;
}

template <class Next>
struct BatchNode
{
    BatchNode(Next next)
        : next{std::move(next)}
    {
        buffer_.reserve(impl::buffer_size);
    }

    ~BatchNode() = default;
    BatchNode(const BatchNode &) = delete;
    auto operator=(const BatchNode &) -> BatchNode & = delete;
    BatchNode(BatchNode &&) = default;
    auto operator=(BatchNode &&) -> BatchNode & = default;

    [[nodiscard]] auto operator()(std::string_view data) noexcept -> std::expected<std::size_t, std::string>
    {
        auto written = std::size_t{};

        try
        {
            buffer_.append_range(data);

            if (std::ranges::size(buffer_) >= impl::buffer_size)
            {
                const auto res = next(buffer_, false);
                if (!res)
                {
                    return std::unexpected(res.error());
                }

                written += *res;
                buffer_.clear();
            }
        }
        catch (const std::exception &e)
        {
            return std::unexpected(e.what());
        }
        catch (...)
        {
            return std::unexpected("unknown exception"s);
        }

        return written;
    }

    auto operator()() -> std::expected<std::size_t, std::string>
    {
        const auto res = next(buffer_, false);
        if (!res)
        {
            return std::unexpected(res.error());
        }

        return std::ranges::size(buffer_);
    }

    Next next;
    std::string buffer_;
};

struct Batch : BaseTransform<BatchNode>
{
};

static_assert(IsTransform<Batch>);

}
