#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace bac::app {

struct AppOptions {
  std::optional<int> frames;      // --frames N: run N frames then exit
  std::optional<std::string> screenshot;  // --screenshot PATH: save to PATH
  std::optional<uint64_t> seed;   // --seed N: RNG seed
  std::optional<std::string> script;      // --script "CMDS": scripted input
};

// Parse command-line arguments into AppOptions
// Throws std::invalid_argument on parse error.
AppOptions parseOptions(int argc, char** argv);

}  // namespace bac::app
