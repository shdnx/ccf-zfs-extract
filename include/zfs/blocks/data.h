#pragma once

#include <memory>

#include "utils/array_view.h"

#include "zfs/general.h"

namespace zfs {

struct DataBlock {
  explicit DataBlock(size_t size) : m_size{size} { ASSERT0(size > 0); }

  virtual ~DataBlock() {}

  size_t size() const { return m_size; }

  void *      data() { return m_data.get(); }
  const void *data() const { return m_data.get(); }

  bool isAllocated() const { return m_data; }

  bool allocate() {
    if (isAllocated())
      return true;

    m_data = std::make_unique(new char[m_size]);
    return m_data;
  }

  template <typename T>
  ArrayView<T> dataAsArray() {
    return ArrayView<T>{m_data.get(), m_size};
  }

private:
  std::unique_ptr<char[]> m_data;
  size_t                  m_size;
};

} // end namespace zfs
