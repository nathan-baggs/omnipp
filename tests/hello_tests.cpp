#include <gtest/gtest.h>

#include "hello.h"

TEST(hello, simple)
{
    ASSERT_EQ(hello::get_message(), "hello world");
}
