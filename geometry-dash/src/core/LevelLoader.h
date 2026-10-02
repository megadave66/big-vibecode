#pragma once
// JSON -> Level. Enforces the palette in docs/LEVEL-FORMAT.md. Owner: level-format section.

#include <optional>
#include <string>
#include <vector>

#include "Level.h"

namespace gd {

// Parse a level from JSON text. On failure returns nullopt and appends human-readable
// messages ("objects[12].w: must be > 0") to `errors`. On success objects are sorted by x
// and endX is set.
std::optional<Level> loadLevelFromString(const std::string& json, std::vector<std::string>& errors);
std::optional<Level> loadLevelFromFile(const std::string& path, std::vector<std::string>& errors);

// Path helpers: "<dataDir>/levels/levelNN.json", "<dataDir>/assets/<rel>".
std::string levelPath(const std::string& dataDir, int id);
std::string assetPath(const std::string& dataDir, const std::string& rel);

}  // namespace gd
