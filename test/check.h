#pragma once

#include <cstdio>
#include <cstdlib>

// テストフレームワークを足すほどの規模ではないので、これだけで済ませる。
#define CHECK(cond)                                                                 \
  do {                                                                              \
    if (!(cond)) {                                                                  \
      std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
      std::exit(1);                                                                 \
    }                                                                               \
  } while (0)
