#pragma once

#include <cstdio>
#include <string>

struct File {
  // NOTE: doesn't handle binary mode, that's important on Windows
  /* unscoped */ enum Mode {
    Read,
    Write,
    Update,
    Overwrite,
    Append,
    UpdateAppend,
  };

  explicit File(const char *path, const char *mode) {
    m_fp = std::fopen(path, mode);
  }

  explicit File(const std::string &path, Mode mode)
      : File(path.c_str(), getModeStr(mode)) {}

  File(const File &other) = delete;
  File &operator=(const File &other) = delete;

  File(File &&from) : m_fp{from.m_fp} { from.m_fp = nullptr; }

  FILE *getHandle() const { return m_fp; }

  operator FILE *() const { return getHandle(); }
  operator bool() const { return static_cast<bool>(getHandle()); }

  ~File() {
    if (m_fp)
      std::fclose(m_fp);
  }

private:
  static const char *getModeStr(Mode m) noexcept;

  FILE *m_fp = nullptr;
};
