# Using `cstring_view`

The `cstring_view` proposal creates a new type that is a null-terminated string view. To use the library in your code,
add the project in your cmakefiles:

```cmake
add_subdirectory(beman/cstring_view)
```

and add the `beman::cstring_view` project to your target

```cmake
target_link_libraries(my_project PRIVATE beman/cstring_view)
```

Subsequently include the `cstring_view` header in your code

```cpp
#include <beman/cstring_view/cstring_view.hpp>
```

From this point on, you're able to use the `cstring_view` type to handle cases where you have NUL-terminated strings.

## Simple examples

Creating a `cstring_view` can be done from a `std::string`, from a string literal, or from a pointer or buffer that is
known to contain a NUL-terminated string. For example:

```cpp
cstring_view v = "Hello World!";
std::string v2 = "Hello World!";
cstring_view v3(v2);
```

Such a `cstring_view` can be used in places where a NUL-terminated string is expected using the `c_str()` function:

```cpp
puts(v.c_str());
```
