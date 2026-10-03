# JsonAdapter

A C++20 JSON type backed by RapidJSON, with an nlohmann-shaped call surface:
`operator[]`, `get<T>`, `contains`, `dump`, and `parse`.

## Include contract

One checkout of this repo is shared by every consumer. Add this directory to
the include path:

```cpp
#include "Json.h"
```

The `jsonadapter` CMake target exports that directory and `rapidjson/include`.
Link it instead of compiling `Json.cpp` into each binary.

## RapidJSON

`rapidjson` is a nested submodule, pinned to commit
`24b5e7a8b27f42fa16b96fc70aade9106cf7102f`
(Tencent/rapidjson, 2025-02-05). That commit is past the v1.1.0 tag and
includes the later ParseNumber fixes.

`Json.cpp` compiles RapidJSON in the namespace `jsonadapter_rapidjson`.
Cereal ships another RapidJSON in the namespace `rapidjson`. Those two
namespaces have to stay distinct, or the copies collide at link time.

## Behavior

`parse(text)` throws `Json::parse_error` when the text is malformed.
`parse(text, callback, false)` returns a discarded value instead. The callback
pointer is ignored.

Non-finite numbers parse and write (`Inf`, `Infinity`, `NaN`, and the same
tokens with a leading minus), so a cereal document that contains them
round-trips.

Object and array nesting deeper than 64 levels is rejected before the parser
runs. RapidJSON's recursive parser overflows the stack on a few dozen
kilobytes of nested brackets.

`Json` throws `Json::exception`. The nlohmann-shaped API needs that.

## Build

```bash
git submodule update --init --recursive
cmake -B build -S .
cmake --build build
ctest --test-dir build --output-on-failure
```
