#pragma once

#include <new>

#include "utils/array_view.h"

#include "zfs/general.h"

namespace zfs {

// TODO: should BlockPtr be owning? the BlockRef, BlockCRef solution is quite
// ugly...

struct DataBlock {
  explicit DataBlock(void *data, size_t dataSize)
      : m_data{data}, m_size{dataSize} {}

  template <typename TBlock>
  explicit DataBlock(size_t dataSize)
      : DataBlock{static_cast<TBlock *>(this) + 1, dataSize} {}

  virtual ~DataBlock() {}

  size_t size() const { return m_size; }

  void *      data() { return m_data; }
  const void *data() const { return m_data; }

  /*template <typename T>
  T &asObject() {
    return *reinterpret_cast<T *>(data());
  }

  template <typename T>
  const T &asObject() {
    return *reinterpret_cast<const T *>(data());
  }

  template <typename T>
  ArrayView<T> asArray() {
    return ArrayView<T>{m_data.get(), m_size};
  }*/

private:
  void * m_data;
  size_t m_size;
};

// An owning block pointer.
struct BlockPtr {
  static BlockPtr allocateRaw(size_t dataSize) {
    char *data = new char[sizeof(DataBlock) + dataSize];
    if (!data)
      return BlockPtr{};

    new (data) DataBlock{dataSize};
    return BlockPtr{data};
  }

  BlockPtr() : m_ptr{nullptr} {}
  explicit BlockPtr(DataBlock *ptr) : m_ptr{ptr} {}

  BlockPtr(const BlockPtr &) = delete;
  BlockPtr(BlockPtr &&rhs) : m_ptr{rhs.m_ptr} { rhs.m_ptr = nullptr; }
  virtual ~BlockPtr() { destroy(); }

  BlockPtr &operator=(const BlockPtr &rhs) = delete;

  BlockPtr &operator=(BlockPtr &&rhs) {
    destroy();
    m_ptr     = rhs.m_ptr;
    rhs.m_ptr = nullptr;
    return *this;
  }

  size_t      size() const { return m_ptr->size(); }
  void *      data() { return m_ptr->data(); }
  const void *data() const { return m_ptr->data(); }

  /* implicit */ operator bool() const { return m_ptr; }

  void destroy() { delete[] reinterpret_cast<char *>(m_ptr); }

private:
  DataBlock *m_ptr;
};

using BlockRef  = BlockPtr &;
using BlockCRef = const BlockPtr &;

struct IndirectBlockPtr : BlockPtr {};

template <typename T>
struct ObjBlockPtr;

template <typename T>
struct ObjBlockPtr<T> : BlockPtr {
  ObjBlockPtr() = default;

  explicit ObjBlockPtr(DataBlock *ptr) : BlockPtr{ptr} {}
  ObjBlockPtr(BlockPtr &&rhs)    = default;
  ObjBlockPtr(ObjBlockPtr &&rhs) = default;

  ObjBlockPtr &operator=(BlockPtr &&rhs) = default;
  ObjBlockPtr &operator=(ObjBlockPtr &&rhs) = default;

  T *      object() { reinterpret_cast<T *>(data()) }
  const T *object() const { return reinterpret_cast<const T *>(data()); }

  T *operator->() { return object(); }
  const T *operator->() const { return object(); }

  T &operator*() { return *object(); }
  const T &operator*() const { return *object(); }
};

template <typename T>
struct ObjBlockPtr<T[]> : BlockPtr {
  ObjBlockPtr() = default;

  explicit ObjBlockPtr(DataBlock *ptr) : BlockPtr{ptr} {}

  ObjBlockPtr(BlockPtr &&rhs)    = default;
  ObjBlockPtr(ObjBlockPtr &&rhs) = default;

  ObjBlockPtr &operator=(BlockPtr &&rhs) = default;
  ObjBlockPtr &operator=(ObjBlockPtr &&rhs) = default;

  ArrayView<T> objects() { return ArrayView<T>{data(), size()}; }
  size_t       numObjects() { return objects().size(); }
};

template <typename T>
using ObjBlockRef = ObjBlockPtr<T> &;

template <typename T>
using ArrayBlockPtr = ObjBlockPtr<T[]>;

template <typename T>
using ArrayBlockRef = ArrayBlockPtr<T> &;

template <typename THeader, typename TEntry>
struct HeadedBlockPtr : BlockPtr {
  HeadedBlockPtr() = default;

  explicit HeadedBlockPtr(DataBlock *ptr) : BlockPtr{ptr} {
    ASSERT0(size() >= sizeof(THeader));
    ASSERT0((size() - sizeof(THeader)) % sizeof(TEntry) == 0);
  }

  HeadedBlockPtr(BlockPtr &&rhs)       = default;
  HeadedBlockPtr(HeadedBlockPtr &&rhs) = default;

  HeadedBlockPtr &operator=(BlockPtr &&rhs) = default;
  HeadedBlockPtr &operator=(HeadedBlockPtr &&rhs) = default;

  THeader *      header() { reinterpret_cast<THeader *>(data()); }
  const THeader *header() const { reinterpret_cast<const THeader *>(data()); }

  THeader *operator->() { return header(); }
  const THeader *operator->() const { return header(); }

  THeader &operator*() { return *header(); }
  const THeader &operator*() const { return *header(); }

  ArrayView<TEntry> entries() {
    return ArrayView<TEntry>{reinterpret_cast<char *>(data()) + sizeof(THeader),
                             size() - sizeof(THeader)};
  }
};

} // end namespace zfs
