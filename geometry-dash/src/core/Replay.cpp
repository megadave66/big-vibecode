#include "core/Replay.h"

#include <charconv>
#include <fstream>
#include <sstream>

#include "core/PhysicsConstants.h"
#include "core/Sim.h"

namespace gd {

namespace {

bool parseInt(const std::string& s, int& out) {
    const char* b = s.data();
    const char* e = s.data() + s.size();
    auto [p, ec] = std::from_chars(b, e, out);
    return ec == std::errc() && p == e;
}

std::string lineMsg(int line, const std::string& msg) {
    return "line " + std::to_string(line) + ": " + msg;
}

}  // namespace

std::optional<Replay> parseReplay(const std::string& text, std::vector<std::string>& errors) {
    Replay r;
    r.tickRate = 0;
    const size_t errStart = errors.size();
    bool haveHeader = false, haveLevel = false, haveRate = false, haveEnd = false;
    bool held = false;
    int lastTick = -1;
    int lineNo = 0;
    int lastLine = 0;

    std::istringstream in(text);
    std::string raw;
    while (std::getline(in, raw)) {
        ++lineNo;
        lastLine = lineNo;
        if (auto h = raw.find('#'); h != std::string::npos) raw.erase(h);
        std::istringstream ls(raw);
        std::vector<std::string> tok;
        for (std::string t; ls >> t;) tok.push_back(t);
        if (tok.empty()) continue;

        const std::string& key = tok[0];
        if (tok.size() != 2) {
            errors.push_back(lineMsg(lineNo, "expected '<keyword> <integer>', got " +
                                                 std::to_string(tok.size()) + " tokens"));
            continue;
        }
        int v = 0;
        if (!parseInt(tok[1], v)) {
            errors.push_back(lineMsg(lineNo, "'" + tok[1] + "' is not an integer"));
            continue;
        }
        if (!haveHeader) {
            if (key != "gdreplay") {
                errors.push_back(lineMsg(lineNo, "first item must be 'gdreplay 1'"));
                return std::nullopt;
            }
            if (v != 1) {
                errors.push_back(lineMsg(lineNo, "unsupported replay version " + std::to_string(v)));
                return std::nullopt;
            }
            haveHeader = true;
            r.version = v;
            continue;
        }
        if (haveEnd) {
            errors.push_back(lineMsg(lineNo, "nothing may follow 'end'"));
            continue;
        }
        if (key == "gdreplay") {
            errors.push_back(lineMsg(lineNo, "duplicate 'gdreplay'"));
        } else if (key == "level") {
            if (haveLevel) errors.push_back(lineMsg(lineNo, "duplicate 'level'"));
            else if (v < 0) errors.push_back(lineMsg(lineNo, "level must be >= 0"));
            haveLevel = true;
            r.level = v;
        } else if (key == "tickrate") {
            if (haveRate) errors.push_back(lineMsg(lineNo, "duplicate 'tickrate'"));
            else if (v != phys::kTickRate)
                errors.push_back(lineMsg(lineNo, "tickrate must be " + std::to_string(phys::kTickRate)));
            haveRate = true;
            r.tickRate = v;
        } else if (key == "press" || key == "release" || key == "end") {
            if (key == "end") haveEnd = true;  // report a bad end tick once, not also "missing"
            if (v < 0) {
                errors.push_back(lineMsg(lineNo, "tick must be >= 0"));
                continue;
            }
            if (v <= lastTick) {
                errors.push_back(lineMsg(lineNo, "tick " + std::to_string(v) +
                                                     " must be greater than previous tick " +
                                                     std::to_string(lastTick)));
                continue;
            }
            if (key == "end") {
                if (v == 0) errors.push_back(lineMsg(lineNo, "end must be > 0"));
                r.endTick = v;
            } else {
                const bool press = key == "press";
                if (press == held) {
                    errors.push_back(lineMsg(lineNo, press ? "press while already held"
                                                           : "release while not held"));
                    continue;
                }
                held = press;
                r.inputs.push_back({v, press});
            }
            lastTick = v;
        } else {
            errors.push_back(lineMsg(lineNo, "unknown keyword '" + key + "'"));
        }
    }
    const int eofLine = lastLine + 1;
    if (!haveHeader) errors.push_back(lineMsg(eofLine, "missing 'gdreplay 1' header"));
    else {
        if (!haveLevel) errors.push_back(lineMsg(eofLine, "missing 'level'"));
        if (!haveRate) errors.push_back(lineMsg(eofLine, "missing 'tickrate'"));
        if (!haveEnd) errors.push_back(lineMsg(eofLine, "missing 'end'"));
    }
    if (errors.size() != errStart) return std::nullopt;
    return r;
}

std::optional<Replay> loadReplayFromFile(const std::string& path, std::vector<std::string>& errors) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        errors.push_back("cannot open " + path);
        return std::nullopt;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    return parseReplay(ss.str(), errors);
}

std::string writeReplay(const Replay& r) {
    std::ostringstream o;
    o << "gdreplay " << r.version << "\n";
    o << "level " << r.level << "\n";
    o << "tickrate " << (r.tickRate ? r.tickRate : phys::kTickRate) << "\n";
    for (const ReplayInput& in : r.inputs) o << (in.press ? "press " : "release ") << in.tick << "\n";
    o << "end " << r.endTick << "\n";
    return o.str();
}

bool saveReplayToFile(const Replay& replay, const std::string& path) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << writeReplay(replay);
    return static_cast<bool>(f);
}

Replay replayFromHeld(int levelId, const std::vector<bool>& held, int endTick) {
    Replay r;
    r.level = levelId;
    r.tickRate = phys::kTickRate;
    r.endTick = endTick;
    bool cur = false;
    for (int t = 0; t < static_cast<int>(held.size()) && t < endTick; ++t) {
        if (held[t] != cur) {
            cur = held[t];
            r.inputs.push_back({t, cur});
        }
    }
    return r;
}

std::vector<bool> heldPerTick(const Replay& r) {
    std::vector<bool> out(r.endTick > 0 ? r.endTick : 0, false);
    bool cur = false;
    size_t k = 0;
    for (int t = 0; t < r.endTick; ++t) {
        while (k < r.inputs.size() && r.inputs[k].tick == t) cur = r.inputs[k++].press;
        out[t] = cur;
    }
    return out;
}

ReplayResult runReplay(const Level& level, const Replay& replay) {
    Sim sim(level);
    bool held = false;
    size_t k = 0;
    while (sim.tick() < replay.endTick && !sim.dead() && !sim.won()) {
        const int t = sim.tick();
        while (k < replay.inputs.size() && replay.inputs[k].tick <= t) held = replay.inputs[k++].press;
        sim.step(held);
    }
    ReplayResult res;
    res.won = sim.won();
    res.died = sim.dead();
    res.tick = sim.tick();
    res.x = sim.player().x;
    return res;
}

}  // namespace gd
