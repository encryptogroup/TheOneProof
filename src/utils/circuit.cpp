#include "circuit.h"

#include <stdexcept>

namespace common::utils {

Gate::Gate(GateType type, wire_t out) : type(type), out(out) {}

FIn2Gate::FIn2Gate(GateType type, wire_t in1, wire_t in2, wire_t out)
    : Gate(type, out), in1{in1}, in2{in2} {}

SIMDGate::SIMDGate(GateType type, std::vector<wire_t> in1,
                   std::vector<wire_t> in2, wire_t out)
    : Gate(type, out), in1(std::move(in1)), in2(std::move(in2)) {}

std::ostream& operator<<(std::ostream& os, GateType type) {
  switch (type) {
    case kInp:
      os << "Input";
      break;

    case kAdd:
      os << "Addition";
      break;

    case kMul:
      os << "Multiplication";
      break;

    case kConstMul:
      os << "Multiplication with constant";
      break;

    case kDotprod:
      os << "Dotproduct";
      break;

    default:
      os << "Invalid";
      break;
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const LevelOrderedCircuit& circ) {
  for (size_t i = 0; i < GateType::NumGates; ++i) {
    os << GateType(i) << ": " << circ.count[i] << "\n";
  }
  os << "Total: " << circ.num_gates << "\n";
  os << "Depth: " << circ.gates_by_level.size() << "\n";
  return os;
}

std::ostream& operator<<(std::ostream& os, const StrictLevelOrderedCircuit& circ) {
  for (size_t i = 0; i < GateType::NumGates; ++i) {
    os << GateType(i) << ": " << circ.count[i] << "\n";
  }
  os << "Total: " << circ.num_gates << "\n";
  os << "Depth: " << circ.gates_by_level.size() << "\n";
  return os;
}
};  // namespace common::utils
