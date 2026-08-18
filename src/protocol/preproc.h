#pragma once

#include "../utils/circuit.h"
#include "sharing.h"
#include "../utils/types.h"

using namespace common::utils;

namespace rzkf {

template <class R>
struct SetupGate {
  SetupGate() = default;
  virtual ~SetupGate() = default;
};

template <class R>
struct SetupInput : public SetupGate<R> {
  R full_lambda;

  SetupInput(R full_lambda) : SetupGate<R>() {
    this->full_lambda = full_lambda;
  }
};

template <class R>
struct SetupMult : public SetupGate<R> {
  R gamma_share;

  SetupMult(R gamma_share) : SetupGate<R>() {
    this->gamma_share = gamma_share;
  }
};

template <class R1, class R2>
struct DealerAllShares : public SetupGate<R1> {
  std::vector<R1> lambda_shares;
  R2 sum_lifted_gamma_minus_lambda;

  DealerAllShares(std::vector<R1> lambda_shares) : SetupGate<R1>() {
    this->lambda_shares = lambda_shares;
    this->sum_lifted_gamma_minus_lambda = 0;
  }

  DealerAllShares(std::vector<R1> lambda_shares, R2 sum_lifted_gamma_minus_lambda) : SetupGate<R1>() {
    this->lambda_shares = lambda_shares;
    this->sum_lifted_gamma_minus_lambda = sum_lifted_gamma_minus_lambda;
  }
};

};  // namespace rzkf
