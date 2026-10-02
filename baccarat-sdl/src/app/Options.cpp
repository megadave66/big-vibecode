#include "Options.hpp"

#include <algorithm>
#include <charconv>
#include <stdexcept>
#include <string_view>

namespace bac::app {

AppOptions parseOptions(int argc, char** argv) {
  AppOptions opts;

  for (int i = 1; i < argc; ++i) {
    std::string_view arg(argv[i]);

    if (arg == "--frames") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--frames requires an argument");
      }
      ++i;
      int frames_val = 0;
      auto result = std::from_chars(argv[i], argv[i] + std::string(argv[i]).length(), frames_val);
      if (result.ec != std::errc()) {
        throw std::invalid_argument(std::string("--frames: invalid number: ") + argv[i]);
      }
      opts.frames = frames_val;
    } else if (arg == "--screenshot") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--screenshot requires an argument");
      }
      ++i;
      opts.screenshot = argv[i];
    } else if (arg == "--seed") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--seed requires an argument");
      }
      ++i;
      uint64_t seed_val = 0;
      auto result = std::from_chars(argv[i], argv[i] + std::string(argv[i]).length(), seed_val);
      if (result.ec != std::errc()) {
        throw std::invalid_argument(std::string("--seed: invalid number: ") + argv[i]);
      }
      opts.seed = seed_val;
    } else if (arg == "--script") {
      if (i + 1 >= argc) {
        throw std::invalid_argument("--script requires an argument");
      }
      ++i;
      opts.script = argv[i];
    } else {
      throw std::invalid_argument(std::string("unknown option: ") + std::string(arg));
    }
  }

  return opts;
}

}  // namespace bac::app
