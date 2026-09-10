#pragma once

// ---------------------------------------------------------------------------
// Minimal Arduino String stand-in for host-side unit tests.
//
// Implements only the subset of the real WString API used by the pure-logic
// headers (json_lite.h, and future header-only helpers). It is NOT a full
// replacement and must never be included by firmware code.
// ---------------------------------------------------------------------------

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>

class String {
 public:
  String() = default;
  String(const char *s) : s_(s != nullptr ? s : "") {}
  String(const std::string &s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}

  unsigned int length() const { return static_cast<unsigned int>(s_.size()); }
  bool isEmpty() const { return s_.empty(); }
  const char *c_str() const { return s_.c_str(); }

  char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : '\0'; }
  char charAt(unsigned int i) const { return (*this)[i]; }

  int indexOf(char c, unsigned int from = 0) const {
    const size_t p = s_.find(c, from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const char *needle, unsigned int from = 0) const {
    const size_t p = s_.find(needle, from);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const String &needle, unsigned int from = 0) const {
    return indexOf(needle.c_str(), from);
  }

  String substring(unsigned int from) const {
    if (from >= s_.size()) return String();
    return String(s_.substr(from));
  }
  String substring(unsigned int from, unsigned int to) const {
    if (from >= s_.size() || to <= from) return String();
    if (to > s_.size()) to = static_cast<unsigned int>(s_.size());
    return String(s_.substr(from, to - from));
  }

  long toInt() const { return std::strtol(s_.c_str(), nullptr, 10); }

  String &operator+=(const String &r) {
    s_ += r.s_;
    return *this;
  }
  String &operator+=(const char *r) {
    if (r != nullptr) s_ += r;
    return *this;
  }
  String &operator+=(char c) {
    s_ += c;
    return *this;
  }
  String &operator+=(int v) {
    s_ += std::to_string(v);
    return *this;
  }
  String &operator+=(unsigned int v) {
    s_ += std::to_string(v);
    return *this;
  }
  String &operator+=(long v) {
    s_ += std::to_string(v);
    return *this;
  }
  String &operator+=(unsigned long v) {
    s_ += std::to_string(v);
    return *this;
  }

  bool equals(const String &o) const { return s_ == o.s_; }
  bool equals(const char *o) const { return s_ == std::string(o != nullptr ? o : ""); }
  bool equalsIgnoreCase(const String &o) const {
    if (s_.size() != o.s_.size()) return false;
    for (size_t i = 0; i < s_.size(); ++i) {
      if (std::tolower(static_cast<unsigned char>(s_[i])) !=
          std::tolower(static_cast<unsigned char>(o.s_[i]))) {
        return false;
      }
    }
    return true;
  }
  bool equalsIgnoreCase(const char *o) const { return equalsIgnoreCase(String(o)); }
  bool startsWith(const String &p) const {
    return s_.size() >= p.s_.size() && s_.compare(0, p.s_.size(), p.s_) == 0;
  }
  bool startsWith(const char *p) const { return startsWith(String(p)); }

  void toCharArray(char *buf, unsigned int size) const {
    if (buf == nullptr || size == 0) return;
    std::strncpy(buf, s_.c_str(), size - 1);
    buf[size - 1] = '\0';
  }
  void reserve(unsigned int n) { s_.reserve(n); }

  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  bool operator==(const char *o) const { return s_ == std::string(o != nullptr ? o : ""); }
  bool operator!=(const char *o) const { return !(*this == o); }

 private:
  std::string s_;
};

inline String operator+(const String &a, const String &b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const String &a, const char *b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const char *a, const String &b) {
  String r(a);
  r += b;
  return r;
}
