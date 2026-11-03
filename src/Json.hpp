#pragma once

#include <cstdlib>
#include <string>
#include <vector>

namespace nt {

class Json {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  Json() = default;
  Json(bool v) : type_(Type::Bool), b_(v) {}
  Json(int v) : type_(Type::Number), n_(v) {}
  Json(long long v) : type_(Type::Number), n_((double)v) {}
  Json(double v) : type_(Type::Number), n_(v) {}
  Json(const char* v) : type_(Type::String), s_(v) {}
  Json(std::string v) : type_(Type::String), s_(std::move(v)) {}

  static Json array() {
    Json j;
    j.type_ = Type::Array;
    return j;
  }
  static Json object() {
    Json j;
    j.type_ = Type::Object;
    return j;
  }

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isArray() const { return type_ == Type::Array; }
  bool isObject() const { return type_ == Type::Object; }
  bool isString() const { return type_ == Type::String; }

  size_t size() const { return vals_.size(); }
  const std::vector<Json>& items() const { return vals_; }
  const std::vector<std::string>& keys() const { return keys_; }

  bool has(const std::string& k) const {
    for (const auto& x : keys_) if (x == k) return true;
    return false;
  }

  const Json& operator[](const std::string& k) const {
    if (type_ == Type::Object) {
      for (size_t i = 0; i < keys_.size(); ++i)
        if (keys_[i] == k) return vals_[i];
    }
    return nullRef();
  }
  const Json& operator[](size_t i) const {
    if (type_ == Type::Array && i < vals_.size()) return vals_[i];
    return nullRef();
  }

  std::string str(const std::string& def = "") const {
    if (type_ == Type::String) return s_;
    if (type_ == Type::Number) return std::to_string((long long)n_);
    return def;
  }
  double num(double def = 0) const {
    if (type_ == Type::Number) return n_;
    if (type_ == Type::String) return std::atof(s_.c_str());
    if (type_ == Type::Bool) return b_ ? 1 : 0;
    return def;
  }
  int integer(int def = 0) const { return type_ == Type::Null ? def : (int)num(def); }
  long long integer64(long long def = 0) const { return type_ == Type::Null ? def : (long long)num((double)def); }
  bool boolean(bool def = false) const {
    if (type_ == Type::Bool) return b_;
    if (type_ == Type::Number) return n_ != 0;
    if (type_ == Type::String) return s_ == "true";
    return def;
  }

  Json& set(const std::string& k, Json v) {
    if (type_ != Type::Object) {
      type_ = Type::Object;
      vals_.clear();
      keys_.clear();
    }
    for (size_t i = 0; i < keys_.size(); ++i) {
      if (keys_[i] == k) {
        vals_[i] = std::move(v);
        return *this;
      }
    }
    keys_.push_back(k);
    vals_.push_back(std::move(v));
    return *this;
  }
  Json& push(Json v) {
    if (type_ != Type::Array) {
      type_ = Type::Array;
      vals_.clear();
      keys_.clear();
    }
    vals_.push_back(std::move(v));
    return *this;
  }

  std::string dump(int indent = -1) const {
    std::string out;
    write(out, indent, 0);
    return out;
  }

  static Json parse(const std::string& text, bool* ok = nullptr) {
    Parser p{text.c_str(), text.c_str() + text.size(), true};
    p.ws();
    Json j = p.value(0);
    p.ws();
    if (p.p != p.e) p.ok = false;
    if (ok) *ok = p.ok;
    return p.ok ? j : Json();
  }

 private:
  Type type_ = Type::Null;
  bool b_ = false;
  double n_ = 0;
  std::string s_;
  std::vector<Json> vals_;
  std::vector<std::string> keys_;

  static const Json& nullRef() {
    static const Json n;
    return n;
  }

  static void escape(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
      switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
          if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
          } else {
            out += (char)c;
          }
      }
    }
    out += '"';
  }

  void write(std::string& out, int indent, int depth) const {
    auto nl = [&](int d) {
      if (indent < 0) return;
      out += '\n';
      out.append((size_t)(d * indent), ' ');
    };
    switch (type_) {
      case Type::Null: out += "null"; break;
      case Type::Bool: out += b_ ? "true" : "false"; break;
      case Type::Number: {
        char buf[40];
        if (n_ == (double)(long long)n_) std::snprintf(buf, sizeof buf, "%lld", (long long)n_);
        else std::snprintf(buf, sizeof buf, "%.10g", n_);
        out += buf;
        break;
      }
      case Type::String: escape(out, s_); break;
      case Type::Array:
        out += '[';
        for (size_t i = 0; i < vals_.size(); ++i) {
          if (i) out += ',';
          nl(depth + 1);
          vals_[i].write(out, indent, depth + 1);
        }
        if (!vals_.empty()) nl(depth);
        out += ']';
        break;
      case Type::Object:
        out += '{';
        for (size_t i = 0; i < vals_.size(); ++i) {
          if (i) out += ',';
          nl(depth + 1);
          escape(out, keys_[i]);
          out += indent < 0 ? ":" : ": ";
          vals_[i].write(out, indent, depth + 1);
        }
        if (!vals_.empty()) nl(depth);
        out += '}';
        break;
    }
  }

  struct Parser {
    const char* p;
    const char* e;
    bool ok;

    void ws() {
      while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) ++p;
    }

    bool lit(const char* s) {
      size_t n = std::char_traits<char>::length(s);
      if ((size_t)(e - p) >= n && std::char_traits<char>::compare(p, s, n) == 0) {
        p += n;
        return true;
      }
      return false;
    }

    static void utf8(std::string& out, unsigned cp) {
      if (cp < 0x80) out += (char)cp;
      else if (cp < 0x800) {
        out += (char)(0xC0 | (cp >> 6));
        out += (char)(0x80 | (cp & 0x3F));
      } else if (cp < 0x10000) {
        out += (char)(0xE0 | (cp >> 12));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
      } else {
        out += (char)(0xF0 | (cp >> 18));
        out += (char)(0x80 | ((cp >> 12) & 0x3F));
        out += (char)(0x80 | ((cp >> 6) & 0x3F));
        out += (char)(0x80 | (cp & 0x3F));
      }
    }

    bool hex4(unsigned& v) {
      if (e - p < 4) return false;
      v = 0;
      for (int i = 0; i < 4; ++i) {
        char c = p[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return false;
      }
      p += 4;
      return true;
    }

    std::string string() {
      std::string out;
      ++p;
      while (p < e && *p != '"') {
        if (*p == '\\') {
          ++p;
          if (p >= e) { ok = false; return out; }
          char c = *p++;
          switch (c) {
            case 'n': out += '\n'; break;
            case 't': out += '\t'; break;
            case 'r': out += '\r'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'u': {
              unsigned v;
              if (!hex4(v)) { ok = false; return out; }
              if (v >= 0xD800 && v < 0xDC00 && e - p >= 6 && p[0] == '\\' && p[1] == 'u') {
                p += 2;
                unsigned lo;
                if (!hex4(lo)) { ok = false; return out; }
                v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00);
              }
              utf8(out, v);
              break;
            }
            default: out += c;
          }
        } else {
          out += *p++;
        }
      }
      if (p >= e) { ok = false; return out; }
      ++p;
      return out;
    }

    Json value(int depth) {
      if (depth > 64 || p >= e) { ok = false; return Json(); }
      char c = *p;
      if (c == '{') {
        ++p;
        Json o = Json::object();
        ws();
        if (p < e && *p == '}') { ++p; return o; }
        while (ok) {
          ws();
          if (p >= e || *p != '"') { ok = false; break; }
          std::string k = string();
          ws();
          if (p >= e || *p != ':') { ok = false; break; }
          ++p;
          ws();
          Json v = value(depth + 1);
          o.keys_.push_back(std::move(k));
          o.vals_.push_back(std::move(v));
          ws();
          if (p < e && *p == ',') { ++p; continue; }
          if (p < e && *p == '}') { ++p; break; }
          ok = false;
        }
        return o;
      }
      if (c == '[') {
        ++p;
        Json a = Json::array();
        ws();
        if (p < e && *p == ']') { ++p; return a; }
        while (ok) {
          ws();
          a.vals_.push_back(value(depth + 1));
          ws();
          if (p < e && *p == ',') { ++p; continue; }
          if (p < e && *p == ']') { ++p; break; }
          ok = false;
        }
        return a;
      }
      if (c == '"') return Json(string());
      if (lit("true")) return Json(true);
      if (lit("false")) return Json(false);
      if (lit("null")) return Json();
      char* end = nullptr;
      double d = std::strtod(p, &end);
      if (end == p) { ok = false; return Json(); }
      p = end;
      return Json(d);
    }
  };
};

}
