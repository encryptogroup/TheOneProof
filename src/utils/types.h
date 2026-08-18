#pragma once
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvla-cxx-extension"
#include <emp-tool/emp-tool.h>
#pragma GCC diagnostic pop
#include <boost/multiprecision/cpp_int.hpp>

#include <cstdint>
#include <iostream>
#include <vector>

namespace common::utils {

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
typedef unsigned __int128 uint128_t;
#pragma GCC diagnostic pop

using Ring = uint32_t;
#define RingSize 32
#define SSEC 40
using Seed = uint128_t;
#ifdef VERIFY_MOD_2_256
using LargeRing = unsigned _BitInt(256); // this is not supported on some platforms
#else
using LargeRing = uint128_t;
#endif

};  // namespace common::utils 
