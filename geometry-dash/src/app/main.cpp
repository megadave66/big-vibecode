#include <cstdio>
#include <string>

#include "App.h"

int main(int argc, char** argv) {
    gd::AppOptions options;
    std::string error;
    if (!gd::parseArgs(argc, argv, options, error)) {
        std::fprintf(stderr, "geometry_dash: %s\n", error.c_str());
        return 2;
    }
    gd::App app(options);
    return app.run();
}
