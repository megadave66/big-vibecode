// JSON -> Level. Strict: unknown keys, wrong types, bad enums and out-of-range values are all
// errors, and every error is collected (not just the first). See docs/LEVEL-FORMAT.md.
#include "core/LevelLoader.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iomanip>
#include <sstream>

#include <nlohmann/json.hpp>

#include "core/PhysicsConstants.h"

namespace gd {
namespace {

using json = nlohmann::json;

struct Ctx {
    std::vector<std::string>& errors;
    void err(const std::string& path, const std::string& msg) {
        errors.push_back(path.empty() ? msg : path + ": " + msg);
    }
};

std::string join(const std::string& base, const std::string& key) {
    return base.empty() ? key : base + "." + key;
}

std::string num(double v) {
    std::ostringstream os;
    os << std::setprecision(10) << v;
    return os.str();
}

bool isHalf(double v) { return std::isfinite(v) && v * 2.0 == std::round(v * 2.0); }

void checkKeys(const json& obj, const std::string& path, std::initializer_list<const char*> allowed,
               Ctx& c) {
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        bool ok = false;
        for (const char* a : allowed)
            if (it.key() == a) ok = true;
        if (!ok) c.err(join(path, it.key()), "unknown key");
    }
}

// Returns the member or nullptr. Logs "missing required field" when required.
const json* field(const json& obj, const char* key, const std::string& path, bool required, Ctx& c) {
    auto it = obj.find(key);
    if (it == obj.end()) {
        if (required) c.err(join(path, key), "missing required field");
        return nullptr;
    }
    return &*it;
}

bool readInt(const json& obj, const char* key, const std::string& path, bool required, int lo,
             int hi, int& out, Ctx& c) {
    const json* v = field(obj, key, path, required, c);
    if (!v) return false;
    const std::string p = join(path, key);
    if (!v->is_number_integer()) {
        c.err(p, std::string("must be an integer (got ") + v->type_name() + ")");
        return false;
    }
    const long long n = v->get<long long>();
    if (n < lo || n > hi) {
        c.err(p, "must be in " + std::to_string(lo) + ".." + std::to_string(hi) + " (got " +
                     std::to_string(n) + ")");
        return false;
    }
    out = static_cast<int>(n);
    return true;
}

bool readNumber(const json& obj, const char* key, const std::string& path, bool required,
                double& out, Ctx& c) {
    const json* v = field(obj, key, path, required, c);
    if (!v) return false;
    if (!v->is_number()) {
        c.err(join(path, key), std::string("must be a number (got ") + v->type_name() + ")");
        return false;
    }
    out = v->get<double>();
    return true;
}

bool readString(const json& obj, const char* key, const std::string& path, bool required,
                std::string& out, Ctx& c) {
    const json* v = field(obj, key, path, required, c);
    if (!v) return false;
    if (!v->is_string()) {
        c.err(join(path, key), std::string("must be a string (got ") + v->type_name() + ")");
        return false;
    }
    out = v->get<std::string>();
    return true;
}

template <class T>
bool readEnum(const json& obj, const char* key, const std::string& path, bool required,
              std::initializer_list<std::pair<const char*, T>> table, T& out, Ctx& c) {
    std::string s;
    if (!readString(obj, key, path, required, s, c)) return false;
    for (const auto& [name, val] : table)
        if (s == name) {
            out = val;
            return true;
        }
    std::string list;
    for (const auto& [name, val] : table) list += (list.empty() ? "" : ", ") + std::string(name);
    c.err(join(path, key), "must be one of " + list + " (got \"" + s + "\")");
    return false;
}

std::size_t utf8Length(const std::string& s) {
    std::size_t n = 0;
    for (unsigned char ch : s)
        if ((ch & 0xC0) != 0x80) ++n;
    return n;
}

bool parseHex(const std::string& s, Color& out) {
    if (s.size() != 7 || s[0] != '#') return false;
    int v[3];
    for (int i = 0; i < 3; ++i) {
        int n = 0;
        for (int k = 0; k < 2; ++k) {
            const char ch = s[1 + i * 2 + k];
            int d;
            if (ch >= '0' && ch <= '9') d = ch - '0';
            else if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
            else if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
            else return false;
            n = n * 16 + d;
        }
        v[i] = n;
    }
    out.r = static_cast<std::uint8_t>(v[0]);
    out.g = static_cast<std::uint8_t>(v[1]);
    out.b = static_cast<std::uint8_t>(v[2]);
    return true;
}

// A coordinate: number, multiple of 0.5, >= minValue (or > 0 when positive == true).
bool readCoord(const json& obj, const char* key, const std::string& path, bool required,
               double def, bool positive, double& out, Ctx& c) {
    double v = def;
    const bool present = obj.contains(key);
    if (!present) {
        if (required) {
            c.err(join(path, key), "missing required field");
            return false;
        }
        out = def;
        return true;
    }
    if (!readNumber(obj, key, path, required, v, c)) return false;
    const std::string p = join(path, key);
    if (!std::isfinite(v)) {
        c.err(p, "must be finite");
        return false;
    }
    if (positive ? !(v > 0) : v < 0) {
        c.err(p, positive ? "must be > 0 (got " + num(v) + ")" : "must be >= 0 (got " + num(v) + ")");
        return false;
    }
    if (!isHalf(v)) {
        c.err(p, "must be a multiple of 0.5 (got " + num(v) + ")");
        return false;
    }
    out = v;
    return true;
}

struct ParseState {
    int endWalls = 0;
    std::size_t endIndex = 0;
};

void parseObject(const json& j, std::size_t index, double ceiling, std::vector<Object>& out,
                 ParseState& ps, Ctx& c) {
    const std::string path = "objects[" + std::to_string(index) + "]";
    if (!j.is_object()) {
        c.err(path, std::string("must be an object (got ") + j.type_name() + ")");
        return;
    }
    std::string typeName;
    if (!readString(j, "type", path, true, typeName, c)) return;

    Object o;
    bool ok = true;
    auto coord = [&](const char* key, bool required, double def, bool positive, double& dst) {
        if (!readCoord(j, key, path, required, def, positive, dst, c)) ok = false;
    };

    if (typeName == "block") {
        o.type = ObjType::Block;
        checkKeys(j, path, {"type", "x", "y", "w", "h"}, c);
        coord("x", true, 0, false, o.x);
        coord("y", true, 0, false, o.y);
        coord("w", false, 1, true, o.w);
        coord("h", false, 1, true, o.h);
    } else if (typeName == "platform") {
        o.type = ObjType::Platform;
        checkKeys(j, path, {"type", "x", "y", "w"}, c);
        coord("x", true, 0, false, o.x);
        coord("y", true, 0, false, o.y);
        coord("w", false, 1, true, o.w);
        o.h = phys::kPlatformHeight;
    } else if (typeName == "spike") {
        o.type = ObjType::Spike;
        checkKeys(j, path, {"type", "x", "y", "dir"}, c);
        coord("x", true, 0, false, o.x);
        coord("y", true, 0, false, o.y);
        if (j.contains("dir") &&
            !readEnum<SpikeDir>(j, "dir", path, false,
                                {{"up", SpikeDir::Up}, {"down", SpikeDir::Down}}, o.spikeDir, c))
            ok = false;
        o.w = o.h = 1;
    } else if (typeName == "portal_gravity" || typeName == "portal_mode" ||
               typeName == "portal_speed") {
        if (typeName == "portal_gravity") {
            o.type = ObjType::PortalGravity;
            checkKeys(j, path, {"type", "x", "y", "gravity"}, c);
            if (!readEnum<Gravity>(j, "gravity", path, true,
                                   {{"down", Gravity::Down}, {"up", Gravity::Up}}, o.gravity, c))
                ok = false;
        } else if (typeName == "portal_mode") {
            o.type = ObjType::PortalMode;
            checkKeys(j, path, {"type", "x", "y", "mode"}, c);
            if (!readEnum<GameMode>(j, "mode", path, true,
                                    {{"cube", GameMode::Cube}, {"ship", GameMode::Ship}}, o.mode, c))
                ok = false;
        } else {
            o.type = ObjType::PortalSpeed;
            checkKeys(j, path, {"type", "x", "y", "speed"}, c);
            if (!readEnum<Speed>(j, "speed", path, true,
                                 {{"slow", Speed::Slow},
                                  {"normal", Speed::Normal},
                                  {"fast", Speed::Fast},
                                  {"faster", Speed::Faster}},
                                 o.speed, c))
                ok = false;
        }
        coord("x", true, 0, false, o.x);
        coord("y", false, 0, false, o.y);
        o.w = 1;
        o.h = phys::kPortalDrawHeight;
    } else if (typeName == "end_wall") {
        o.type = ObjType::EndWall;
        checkKeys(j, path, {"type", "x"}, c);
        coord("x", true, 0, false, o.x);
        o.y = 0;
        o.w = 1;
        o.h = ceiling;
        ++ps.endWalls;
        if (ps.endWalls == 1) ps.endIndex = index;
        else c.err(path, "more than one end_wall (first is objects[" + std::to_string(ps.endIndex) + "])");
    } else if (typeName == "deco") {
        o.type = ObjType::Deco;
        checkKeys(j, path, {"type", "x", "y", "w", "h", "kind"}, c);
        coord("x", true, 0, false, o.x);
        coord("y", true, 0, false, o.y);
        coord("w", false, 1, true, o.w);
        coord("h", false, 1, true, o.h);
        if (!readEnum<DecoKind>(j, "kind", path, true,
                                {{"pillar", DecoKind::Pillar},
                                 {"chain", DecoKind::Chain},
                                 {"star", DecoKind::Star},
                                 {"arrow", DecoKind::Arrow},
                                 {"glow", DecoKind::Glow}},
                                o.deco, c))
            ok = false;
    } else {
        c.err(join(path, "type"),
              "unknown object type \"" + typeName +
                  "\" (expected block, platform, spike, portal_gravity, portal_mode, "
                  "portal_speed, end_wall, deco)");
        return;
    }

    // Bounds: fully between ground and ceiling.
    if (ok && o.type != ObjType::EndWall && o.y + o.h > ceiling + 1e-9)
        c.err(join(path, "y"), "object top y + h = " + num(o.y + o.h) + " is above the ceiling " +
                                   num(ceiling));
    if (ok || o.type == ObjType::EndWall) out.push_back(o);
}

bool loadImpl(const json& root, Level& lv, Ctx& c) {
    if (!root.is_object()) {
        c.err("$", std::string("top level must be an object (got ") + root.type_name() + ")");
        return false;
    }
    checkKeys(root, "", {"format", "id", "name", "difficulty", "music", "start", "ceiling",
                         "colors", "objects"},
              c);

    readInt(root, "format", "", true, 1, 1, lv.format, c);
    readInt(root, "id", "", true, 1, 10, lv.id, c);
    readInt(root, "difficulty", "", true, 1, 10, lv.difficulty, c);

    if (readString(root, "name", "", true, lv.name, c)) {
        const std::size_t n = utf8Length(lv.name);
        if (n < 1 || n > 32) c.err("name", "must be 1..32 characters (got " + std::to_string(n) + ")");
    }
    if (readString(root, "music", "", true, lv.music, c)) {
        if (lv.music.empty()) c.err("music", "must not be empty");
        else if (lv.music[0] == '/' || lv.music.find("..") != std::string::npos ||
                 lv.music.find('\\') != std::string::npos)
            c.err("music", "must be a relative path under assets/ (no leading '/', no '..')");
    }

    if (auto it = root.find("start"); it != root.end()) {
        if (!it->is_object()) {
            c.err("start", std::string("must be an object (got ") + it->type_name() + ")");
        } else {
            checkKeys(*it, "start", {"mode", "speed", "gravity"}, c);
            if (it->contains("mode"))
                readEnum<GameMode>(*it, "mode", "start", false,
                                   {{"cube", GameMode::Cube}, {"ship", GameMode::Ship}},
                                   lv.startMode, c);
            if (it->contains("speed"))
                readEnum<Speed>(*it, "speed", "start", false,
                                {{"slow", Speed::Slow},
                                 {"normal", Speed::Normal},
                                 {"fast", Speed::Fast},
                                 {"faster", Speed::Faster}},
                                lv.startSpeed, c);
            if (it->contains("gravity"))
                readEnum<Gravity>(*it, "gravity", "start", false,
                                  {{"down", Gravity::Down}, {"up", Gravity::Up}}, lv.startGravity, c);
        }
    }

    lv.ceiling = phys::kDefaultCeiling;
    if (root.contains("ceiling")) {
        double v;
        if (readNumber(root, "ceiling", "", false, v, c)) {
            if (!std::isfinite(v) || v < phys::kMinCeiling || v > phys::kMaxCeiling)
                c.err("ceiling", "must be in " + num(phys::kMinCeiling) + ".." +
                                     num(phys::kMaxCeiling) + " (got " + num(v) + ")");
            else lv.ceiling = v;
        }
    }

    if (auto it = root.find("colors"); it != root.end()) {
        if (!it->is_object()) {
            c.err("colors", std::string("must be an object (got ") + it->type_name() + ")");
        } else {
            checkKeys(*it, "colors", {"bg", "ground", "accent"}, c);
            struct Slot { const char* key; Color* dst; };
            for (const Slot& s : {Slot{"bg", &lv.bg}, Slot{"ground", &lv.ground}, Slot{"accent", &lv.accent}}) {
                std::string str;
                if (it->contains(s.key) && readString(*it, s.key, "colors", false, str, c)) {
                    if (!parseHex(str, *s.dst))
                        c.err(join("colors", s.key), "must be \"#rrggbb\" (got \"" + str + "\")");
                }
            }
        }
    }

    auto objs = root.find("objects");
    if (objs == root.end()) {
        c.err("objects", "missing required field");
        return false;
    }
    if (!objs->is_array()) {
        c.err("objects", std::string("must be an array (got ") + objs->type_name() + ")");
        return false;
    }
    ParseState ps;
    std::vector<Object> parsed;
    std::vector<std::size_t> sourceIndex;
    for (std::size_t i = 0; i < objs->size(); ++i) {
        const std::size_t before = parsed.size();
        parseObject((*objs)[i], i, lv.ceiling, parsed, ps, c);
        if (parsed.size() > before) sourceIndex.push_back(i);
    }
    if (ps.endWalls == 0) {
        c.err("objects", "missing end_wall (exactly one is required)");
    } else if (ps.endWalls == 1) {
        for (std::size_t k = 0; k < parsed.size(); ++k) {
            if (parsed[k].type == ObjType::EndWall) lv.endX = parsed[k].x;
        }
        for (std::size_t k = 0; k < parsed.size(); ++k) {
            if (parsed[k].type != ObjType::EndWall && parsed[k].x > lv.endX)
                c.err("objects[" + std::to_string(sourceIndex[k]) + "].x",
                      "object is right of the end_wall at x=" + num(lv.endX) +
                          " (end_wall must be the right-most object)");
        }
    }
    lv.objects = std::move(parsed);
    return true;
}

}  // namespace

std::optional<Level> loadLevelFromString(const std::string& text, std::vector<std::string>& errors) {
    Ctx c{errors};
    const std::size_t before = errors.size();
    json root;
    try {
        root = json::parse(text);
    } catch (const json::parse_error& e) {
        c.err("json", e.what());
        return std::nullopt;
    }
    Level lv;
    loadImpl(root, lv, c);
    if (errors.size() != before) return std::nullopt;
    std::stable_sort(lv.objects.begin(), lv.objects.end(),
                     [](const Object& a, const Object& b) { return a.x < b.x; });
    return lv;
}

std::optional<Level> loadLevelFromFile(const std::string& path, std::vector<std::string>& errors) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        errors.push_back("cannot open file: " + path);
        return std::nullopt;
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::size_t before = errors.size();
    auto lv = loadLevelFromString(ss.str(), errors);
    if (!lv) return std::nullopt;

    // File name levelNN.json must match "id".
    const std::size_t slash = path.find_last_of("/\\");
    const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    if (base.size() == 12 && base.compare(0, 5, "level") == 0 && base.compare(7, 5, ".json") == 0 &&
        std::isdigit(static_cast<unsigned char>(base[5])) &&
        std::isdigit(static_cast<unsigned char>(base[6]))) {
        const int fileNo = (base[5] - '0') * 10 + (base[6] - '0');
        if (fileNo != lv->id)
            errors.push_back("id: " + std::to_string(lv->id) + " does not match file name " + base);
    }
    if (errors.size() != before) return std::nullopt;
    return lv;
}

std::string levelPath(const std::string& dataDir, int id) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "level%02d.json", id);
    return dataDir + "/levels/" + buf;
}

std::string assetPath(const std::string& dataDir, const std::string& rel) {
    return dataDir + "/assets/" + rel;
}

}  // namespace gd
