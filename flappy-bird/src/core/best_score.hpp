// Best-score persistence. Pure std, no SDL. The caller picks the path
// (the app uses SDL_GetPrefPath; tests use a temp dir).
#pragma once

#include <filesystem>

namespace flappy {

// Returns the stored best score, or 0 when the file is missing, unreadable,
// empty, corrupt, or holds a negative number.
int load_best(const std::filesystem::path& file);

// Writes `best` as plain text (e.g. "17\n"). Creates parent dirs if needed.
// Writes to a temp file then renames, so a crash never leaves a half file.
// Returns false on any I/O failure (never throws).
bool save_best(const std::filesystem::path& file, int best);

}  // namespace flappy
