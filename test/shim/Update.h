#pragma once

#include <Arduino.h>

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF

class MockUpdateClass {
public:
  bool begin_ret = true;
  bool end_ret = true;
  size_t write_ret = 0;
  String error_str = "";
  String md5 = "";
  bool abort_called = false;

  bool begin(size_t) { return begin_ret; }
  void setMD5(const char* m) { this->md5 = m; }
  size_t write(uint8_t*, size_t) { return write_ret; }
  bool end(bool) { return end_ret; }
  void abort() { abort_called = true; }
  const char* errorString() { return error_str.c_str(); }
};

extern MockUpdateClass Update;
