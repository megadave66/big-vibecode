// gd_validate <level.json>... [--data DIR]
// Loads and validates each level. Exit 0 only if every file is OK.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/LevelLoader.h"
#include "core/Validator.h"

int main(int argc, char** argv) {
    std::vector<std::string> files;
#ifdef GD_DATA_DIR
    std::string dataDir = GD_DATA_DIR;
#else
    std::string dataDir;
#endif
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--data") == 0) {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "gd_validate: --data needs a directory\n");
                return 2;
            }
            dataDir = argv[++i];
        } else if (argv[i][0] == '-' && argv[i][1] == '-') {
            std::fprintf(stderr, "gd_validate: unknown option %s\n", argv[i]);
            return 2;
        } else {
            files.push_back(argv[i]);
        }
    }
    if (files.empty()) {
        std::fprintf(stderr, "usage: gd_validate <level.json>... [--data DIR]\n");
        return 2;
    }

    bool allOk = true;
    for (const std::string& path : files) {
        const std::size_t slash = path.find_last_of("/\\");
        const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        std::vector<std::string> errors;
        auto lv = gd::loadLevelFromFile(path, errors);
        if (!lv) {
            allOk = false;
            for (const std::string& e : errors) std::printf("%s: [load] %s\n", name.c_str(), e.c_str());
            continue;
        }
        const gd::ValidationReport rep = gd::validateLevel(*lv, dataDir);
        if (rep.ok()) {
            std::printf("OK %s (%.1f s, %zu objects)\n", name.c_str(), rep.durationSeconds,
                        lv->objects.size());
            continue;
        }
        allOk = false;
        for (const gd::ValidationIssue& is : rep.issues) {
            if (is.x >= 0)
                std::printf("%s: [%s] x=%.1f: %s\n", name.c_str(), is.rule.c_str(), is.x,
                            is.message.c_str());
            else
                std::printf("%s: [%s] %s\n", name.c_str(), is.rule.c_str(), is.message.c_str());
        }
    }
    return allOk ? 0 : 1;
}
