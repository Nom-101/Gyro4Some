#pragma once
// A sensitivity profile driven by the game's own state (<Key>ProfileMode=Game, <Key>ProfileCondition=): the profile
// is on while a property the engine reads each frame satisfies a comparison, instead of while a button is held.
//
//   <Key>ProfileCondition=<path>                   the property is true / non-zero
//   <Key>ProfileCondition=<path> <op> <value>      op: == != < <= > >=   value: a number, true or false
//
// The path's syntax belongs to the engine module (Unreal: Root.Sub.Property, roots Pawn, Controller, CameraManager;
// see docs/ADVANCED.md). The core only parses the comparison and evaluates it on the number the engine reads.
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace gyro4all {

struct ProfileCondition {
    enum class Op { NonZero, Eq, Ne, Lt, Le, Gt, Ge };
    uint32_t view_id = 0;  // the profile's view (one of kSensitivityViewIds)
    std::string path;
    Op op = Op::NonZero;
    double value = 0;

    bool holds(double v) const {
        switch (op) {
            case Op::NonZero: return v != 0;
            case Op::Eq: return v == value;
            case Op::Ne: return v != value;
            case Op::Lt: return v < value;
            case Op::Le: return v <= value;
            case Op::Gt: return v > value;
            case Op::Ge: return v >= value;
        }
        return false;
    }

    std::string describe() const {
        static const char* const names[] = {"", "==", "!=", "<", "<=", ">", ">="};
        if (op == Op::NonZero) return path;
        char number[32];
        std::snprintf(number, sizeof number, "%g", value);
        return path + " " + names[int(op)] + " " + number;
    }
};

namespace detail {
inline std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}
}  // namespace detail

// Parses "<path> [<op> <value>]". False (with `error` set) when the text is empty, the path has spaces or other
// characters a property path can't have, the operator is unknown or the value isn't a number / true / false.
inline bool parse_profile_condition(const std::string& text, ProfileCondition& out, std::string* error = nullptr) {
    const auto fail = [&](const char* why) {
        if (error) *error = why;
        return false;
    };
    const std::string s = detail::trim(text);
    if (s.empty()) return fail("empty");
    // The operator: the first of  == != <= >= < >  in the text (longest first so "<=" isn't read as "<").
    struct OpText {
        const char* text;
        ProfileCondition::Op op;
    };
    static const OpText ops[] = {{"==", ProfileCondition::Op::Eq}, {"!=", ProfileCondition::Op::Ne}, {"<=", ProfileCondition::Op::Le},
                                 {">=", ProfileCondition::Op::Ge}, {"<", ProfileCondition::Op::Lt},  {">", ProfileCondition::Op::Gt}};
    size_t at = std::string::npos;
    const OpText* found = nullptr;
    for (const auto& o : ops) {
        const size_t pos = s.find(o.text);
        if (pos != std::string::npos && (at == std::string::npos || pos < at || (pos == at && std::strlen(o.text) > std::strlen(found->text)))) {
            at = pos;
            found = &o;
        }
    }
    std::string path = found ? detail::trim(s.substr(0, at)) : s;
    if (path.empty()) return fail("no property path before the operator");
    for (const unsigned char c : path)
        if (!(std::isalnum(c) || c == '_' || c == '.')) return fail("the property path has a character it can't have");
    if (path.front() == '.' || path.back() == '.' || path.find("..") != std::string::npos) return fail("malformed property path");
    out.path = path;
    if (!found) {
        out.op = ProfileCondition::Op::NonZero;
        out.value = 0;
        return true;
    }
    std::string value = detail::trim(s.substr(at + std::strlen(found->text)));
    if (value.empty()) return fail("no value after the operator");
    std::string lower = value;
    for (char& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (lower == "true") out.value = 1;
    else if (lower == "false") out.value = 0;
    else {
        char* end = nullptr;
        out.value = std::strtod(value.c_str(), &end);
        if (end == value.c_str() || *end != '\0' || !std::isfinite(out.value)) return fail("the value isn't a number, true or false");
    }
    out.op = found->op;
    return true;
}

}  // namespace gyro4all
