// test/mocks/FakeMsgLogBackend.h
#pragma once
#include <mishmesh/core/MsgLogBackend.h>
#include <map>
#include <string>
#include <vector>
#include <cstring>

namespace mishmesh {

// In-memory backend for host tests. `capacity` lets tests exercise the backstop;
// default is effectively unbounded.
struct FakeMsgLogBackend : MsgLogBackend {
  std::map<std::string, std::vector<uint8_t>> files;
  std::vector<uint8_t> index;
  uint32_t capacity = 8u * 1024 * 1024;

  uint32_t usedBytes() const {
    uint32_t n = (uint32_t)index.size();
    for (auto& kv : files) n += (uint32_t)kv.second.size();
    return n;
  }
  bool append(const char* name, const uint8_t* rec, uint32_t len) override {
    if (usedBytes() + len > capacity) return false;
    auto& f = files[name];
    f.insert(f.end(), rec, rec + len);
    return true;
  }
  uint32_t size(const char* name) const override {
    auto it = files.find(name);
    return it == files.end() ? 0 : (uint32_t)it->second.size();
  }
  uint32_t read(const char* name, uint32_t off, uint8_t* dst, uint32_t cap) const override {
    auto it = files.find(name);
    if (it == files.end() || off >= it->second.size()) return 0;
    uint32_t n = (uint32_t)it->second.size() - off;
    if (n > cap) n = cap;
    memcpy(dst, it->second.data() + off, n);
    return n;
  }
  bool rewrite(const char* name, const uint8_t* data, uint32_t len) override {
    files[name].assign(data, data + len);
    return true;
  }
  bool remove(const char* name) override { files.erase(name); return true; }
  bool patch(const char* name, uint32_t off, const uint8_t* src, uint32_t len) override {
    auto& f = files[name];
    uint32_t end = off + len;
    if (end > (uint32_t)f.size()) f.resize(end, 0);
    memcpy(f.data() + off, src, len);
    return true;
  }
  bool dropFront(const char* name, uint32_t bytes) override {
    auto it = files.find(name);
    if (it == files.end()) return false;
    auto& f = it->second;
    if (bytes >= (uint32_t)f.size()) { f.clear(); return true; }
    f.erase(f.begin(), f.begin() + bytes);
    return true;
  }
  bool removeRange(const char* name, uint32_t off, uint32_t len) override {
    auto it = files.find(name);
    if (it == files.end()) return false;
    auto& f = it->second;
    if (off >= (uint32_t)f.size()) return true;
    uint32_t end = off + len;
    if (end > (uint32_t)f.size()) end = (uint32_t)f.size();
    f.erase(f.begin() + off, f.begin() + end);
    return true;
  }
  bool truncate(const char* name, uint32_t len) override {
    auto it = files.find(name);
    if (it != files.end() && it->second.size() > len) it->second.resize(len);
    return true;
  }
  int list(MsgLogInfo* out, int cap) const override {
    int n = 0;
    for (auto& kv : files) {
      if (n >= cap) break;
      std::strncpy(out[n].name, kv.first.c_str(), sizeof(out[n].name) - 1);
      out[n].name[sizeof(out[n].name) - 1] = 0;
      out[n].bytes = (uint32_t)kv.second.size();
      n++;
    }
    return n;
  }
  bool saveIndex(const uint8_t* data, uint32_t len) override {
    index.assign(data, data + len); return true;
  }
  uint32_t loadIndex(uint8_t* dst, uint32_t cap) const override {
    uint32_t n = (uint32_t)index.size();
    if (n > cap) n = cap;
    memcpy(dst, index.data(), n);
    return n;
  }
  uint32_t freeBytes() const override {
    uint32_t used = usedBytes();
    return used >= capacity ? 0 : capacity - used;
  }
};

} // namespace mishmesh
