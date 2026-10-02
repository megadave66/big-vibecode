#pragma once
// Static level checks (palette conformance + completability invariants).
// Rules are listed in docs/LEVEL-FORMAT.md. Owner: level-format section.

#include <string>
#include <vector>

#include "Level.h"

namespace gd {

struct ValidationIssue {
    std::string rule;     // "palette", "embedded-hazard", "climb", "spike-run", "ship-gap",
                          // "ship-slope", "duration", "start-clear", "music"
    double x = 0;         // where (blocks), -1 if global
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;
    double durationSeconds = 0;
    bool ok() const { return issues.empty(); }
};

// dataDir may be empty: then the music-file existence check is skipped.
ValidationReport validateLevel(const Level& level, const std::string& dataDir = "");

}  // namespace gd
