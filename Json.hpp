// Minimal strict JSON parser (RFC 8259 subset: no comments, no trailing commas).
//
// Used only for presets.json. Having our own ~150 line parser keeps the preset pipeline
// independent of matjson's API (which changed between Geode majors) and lets the whole
// loader be unit-tested without the game.
#pragma once

#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace adeco::json {

struct Value {
    enum class Type { Null, Bool, Number, String, Array, Object };
    Type type = Type::Null;
    bool b = false;
    double n = 0;
    std::string s;
    std::vector<Value> arr;           // Array items
    std::vector<std::string> keys;    // Object keys (parallel to vals; keeps file order)
    std::vector<Value> vals;          // Object values

    bool isObject() const { return type == Type::Object; }
    bool isArray() const { return type == Type::Array; }
    bool isString() const { return type == Type::String; }
    bool isNumber() const { return type == Type::Number; }
    bool isBool() const { return type == Type::Bool; }

    Value const* find(std::string_view key) const {
        if (type != Type::Object) return nullptr;
        for (size_t i = 0; i < keys.size(); ++i)
            if (keys[i] == key) return &vals[i];
        return nullptr;
    }
};

struct ParseResult {
    Value value;
    bool ok = false;
    std::string error;  // "line:col message"
};

namespace detail {

struct Parser {
    std::string_view t;
    size_t i = 0;
    std::string err;

    bool fail(char const* msg) {
        if (err.empty()) {
            size_t line = 1, col = 1;
            for (size_t k = 0; k < i && k < t.size(); ++k) {
                if (t[k] == '\n') { ++line; col = 1; } else ++col;
            }
            err = std::to_string(line) + ":" + std::to_string(col) + " " + msg;
        }
        return false;
    }
    void ws() {
        while (i < t.size() && (t[i] == ' ' || t[i] == '\t' || t[i] == '\n' || t[i] == '\r')) ++i;
    }
    bool lit(std::string_view w) {
        if (t.substr(i, w.size()) == w) { i += w.size(); return true; }
        return false;
    }
    static void utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F));
        } else {
            out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F));
            out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F));
        }
    }
    bool hex4(unsigned& v) {
        if (i + 4 > t.size()) return fail("bad \\u escape");
        v = 0;
        for (int k = 0; k < 4; ++k) {
            char c = t[i++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= unsigned(c - '0');
            else if (c >= 'a' && c <= 'f') v |= unsigned(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') v |= unsigned(10 + c - 'A');
            else return fail("bad \\u escape");
        }
        return true;
    }
    bool str(std::string& out) {
        if (i >= t.size() || t[i] != '"') return fail("expected string");
        ++i;
        while (i < t.size()) {
            char c = t[i++];
            if (c == '"') return true;
            if (static_cast<unsigned char>(c) < 0x20) return fail("control character in string");
            if (c != '\\') { out += c; continue; }
            if (i >= t.size()) break;
            char e = t[i++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    if (!hex4(cp)) return false;
                    if (cp >= 0xD800 && cp < 0xDC00 && t.substr(i, 2) == "\\u") {
                        i += 2;
                        unsigned lo = 0;
                        if (!hex4(lo)) return false;
                        if (lo < 0xDC00 || lo > 0xDFFF) return fail("bad surrogate pair");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                    }
                    utf8(out, cp);
                    break;
                }
                default: return fail("bad escape");
            }
        }
        return fail("unterminated string");
    }
    bool value(Value& v, int depth) {
        if (depth > 48) return fail("nesting too deep");
        ws();
        if (i >= t.size()) return fail("unexpected end of input");
        char c = t[i];
        if (c == '{') {
            ++i; v.type = Value::Type::Object; ws();
            if (i < t.size() && t[i] == '}') { ++i; return true; }
            for (;;) {
                ws();
                std::string k;
                if (!str(k)) return false;
                ws();
                if (i >= t.size() || t[i] != ':') return fail("expected ':'");
                ++i;
                Value child;
                if (!value(child, depth + 1)) return false;
                v.keys.push_back(std::move(k));
                v.vals.push_back(std::move(child));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == '}') { ++i; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++i; v.type = Value::Type::Array; ws();
            if (i < t.size() && t[i] == ']') { ++i; return true; }
            for (;;) {
                Value child;
                if (!value(child, depth + 1)) return false;
                v.arr.push_back(std::move(child));
                ws();
                if (i < t.size() && t[i] == ',') { ++i; continue; }
                if (i < t.size() && t[i] == ']') { ++i; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { v.type = Value::Type::String; return str(v.s); }
        if (lit("true")) { v.type = Value::Type::Bool; v.b = true; return true; }
        if (lit("false")) { v.type = Value::Type::Bool; v.b = false; return true; }
        if (lit("null")) { v.type = Value::Type::Null; return true; }
        if (c == '-' || (c >= '0' && c <= '9')) {
            size_t st = i;
            if (t[i] == '-') ++i;
            while (i < t.size() && ((t[i] >= '0' && t[i] <= '9') || t[i] == '.' || t[i] == 'e' || t[i] == 'E' ||
                                    t[i] == '+' || t[i] == '-'))
                ++i;
            std::string num(t.substr(st, i - st));
            char* end = nullptr;
            v.n = std::strtod(num.c_str(), &end);
            if (end == num.c_str() || *end != '\0') { i = st; return fail("bad number"); }
            v.type = Value::Type::Number;
            return true;
        }
        return fail("unexpected character");
    }
};

}  // namespace detail

inline ParseResult parse(std::string_view text) {
    ParseResult r;
    detail::Parser p;
    p.t = text;
    if (!p.value(r.value, 0)) { r.error = p.err; return r; }
    p.ws();
    if (p.i != text.size()) { p.fail("trailing characters"); r.error = p.err; return r; }
    r.ok = true;
    return r;
}

}  // namespace adeco::json
