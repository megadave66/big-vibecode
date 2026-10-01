#include "App.hpp"
#include "Options.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
  try {
    auto opts = bac::app::parseOptions(argc, argv);
    auto app = bac::app::App::create(opts);
    if (!app) return 1;
    return app->run();
  } catch (const std::exception& e) {
    std::fprintf(stderr, "baccarat: %s\n", e.what());
    return 2;
  }
}
