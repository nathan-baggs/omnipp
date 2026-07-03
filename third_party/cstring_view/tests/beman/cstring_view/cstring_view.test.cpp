// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>
#include <beman/cstring_view/cstring_view.hpp>
#include <string>
#include <string_view>

using namespace beman::literals;
using namespace std::literals;

TEST(StringView, ConstructionDestruction) {
    std::string         s  = "hello";
    beman::cstring_view h1 = "hello";
    beman::cstring_view h2 = h1;

    EXPECT_EQ(h1.c_str(), h2.c_str());
    EXPECT_NE(h1.c_str(), s.c_str());
    EXPECT_TRUE(h1 == h2);
    EXPECT_TRUE(h1 == s);
    EXPECT_TRUE(s == h1);
#if __cpp_lib_starts_ends_with >= 201711L
    EXPECT_TRUE(h1.starts_with("he"));
    EXPECT_TRUE(h1.ends_with("lo"));
#endif
    EXPECT_TRUE(h1 == "hello");
    EXPECT_TRUE(h1 == "hello"sv);
#if __cpp_lib_three_way_comparison
    EXPECT_EQ(h1 <=> h1, std::strong_ordering::equal);
#endif
    EXPECT_EQ(h1[0], 'h');
    auto first = h1.substr(0, 2);
    auto end   = h1.substr(2);
    EXPECT_TRUE((std::is_same_v<decltype(first), std::string_view>));
    EXPECT_TRUE((std::is_same_v<decltype(end), beman::cstring_view>));
    EXPECT_EQ(first, "he");
    EXPECT_EQ(end, "llo");
}
