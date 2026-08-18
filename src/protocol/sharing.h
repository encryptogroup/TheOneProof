#pragma once

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wpragmas"
#pragma GCC diagnostic ignored "-Wvla-cxx-extension"
#include <emp-tool/emp-tool.h>
#pragma GCC diagnostic pop

#include <array>
#include <vector>

#include "../utils/types.h"

using namespace common::utils;

namespace rzkf {

template <class R>
class StarShare {
  R m_; /// Unused by the Dealer
  R lambda_; /// For Dealer, this is the complete mask, for each other party, it't this party's share of the mask

  public:
    StarShare() = default;
    explicit StarShare(R m, R lambda): m_(m), lambda_(lambda) {};

    R& getM() { return m_; }
    R& getLambda() { return lambda_; }
};

};  // namespace rzkf
