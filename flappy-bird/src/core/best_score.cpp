// Best-score persistence. See best_score.hpp.
#include "best_score.hpp"

#include <cctype>
#include <fstream>
#include <string>
#include <system_error>

namespace flappy {

int load_best(const std::filesystem::path& file) {
    try {
        std::ifstream in(file);
        if (!in) {
            return 0;
        }
        std::string line;
        std::getline(in, line);
        std::size_t i = 0;
        while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i])) != 0) {
            ++i;
        }
        long long v = 0;
        std::size_t digits = 0;
        while (i < line.size() && line[i] >= '0' && line[i] <= '9') {
            v = v * 10 + (line[i] - '0');
            if (v > 1000000000) {
                return 0;
            }
            ++i;
            ++digits;
        }
        // A leading '-' or any non-digit start leaves digits == 0 -> 0.
        if (digits == 0) {
            return 0;
        }
        return static_cast<int>(v);
    } catch (...) {
        return 0;
    }
}

bool save_best(const std::filesystem::path& file, int best) {
    try {
        namespace fs = std::filesystem;
        std::error_code ec;
        const fs::path parent = file.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, ec);
            if (ec) {
                return false;
            }
        }
        fs::path tmp = file;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::trunc);
            if (!out) {
                return false;
            }
            out << best << '\n';
            out.flush();
            if (!out) {
                fs::remove(tmp, ec);
                return false;
            }
        }
        fs::rename(tmp, file, ec);
        if (ec) {
            std::error_code ignore;
            fs::remove(tmp, ignore);
            return false;
        }
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace flappy
