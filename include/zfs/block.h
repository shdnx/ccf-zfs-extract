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

private:
  void * m_data;
  size_t m_size;
};

// An owning block pointer.
// NOTE: this class can be derived from, but derived classes CANNOT add any
// extra fields, or increase the size in any way. It has to be possible for a
// BlockPtr to be reinterpreted as an object of any child class.
struct BlockPtr {
  static const BlockPtr null;

  template <typename T>
  static T allocate(size_t dataSize) {
    ASSERT0(sizeof(T) == sizeof(BlockPtr));

    char *data = new char[sizeof(DataBlock) + dataSize];
    if (!data)
      return T{};

    return T{new (data) DataBlock{data + sizeof(DataBlock), dataSize}};
  }

  static BlockPtr allocate(size_t dataSize) {
    return allocate<BlockPtr>(dataSize);
  }

  BlockPtr() : m_ptr{nullptr} {}
  /* implicit */ BlockPtr(std::nullptr_t) : BlockPtr{} {}

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

  void destroy() {
    if (m_ptr)
      delete[] reinterpret_cast<char *>(m_ptr);
  }

  template <typename T>
  T &getAs() {
    ASSERT0(sizeof(T) == sizeof(BlockPtr));
    return *static_cast<T *>(this);
  }

  template <typename T>
  const T &getAs() {
    ASSERT0(sizeof(T) == sizeof(BlockPtr));
    return *static_cast<const T *>(this);
  }

  template <typename T>
  T cast() && {
    ASSERT0(sizeof(T) == sizeof(BlockPtr));
    return static_cast<T>(std::move(*this));
  }

private:
  DataBlock *m_ptr;
};

using BlockRef  = BlockPtr &;
using BlockCRef = const BlockPtr &;

#define GEN_BLOCKPTR_SUPPORT(NAME, BASE)         \
  static NAME allocate(std::size_t dataSize) {   \
    return BlockPtr::allocate<NAME>(dataSize);   \
  }                                              \
                                                 \
  NAME() = default;                              \
  NAME(std::nullptr_t) : NAME{} {}               \
  NAME(BlockPtr &&rhs) : BASE{std::move(rhs)} {} \
  /*NAME(NAME &&rhs) = default;*/                \
  NAME &operator=(BlockPtr &&rhs) {              \
    BASE::operator=(std::move(rhs));             \
    return *this;                                \
  }                                              \
  /*NAME &operator=(NAME &&rhs) = default;*/     \
  explicit NAME(DataBlock *ptr) : BASE { ptr }

template <typename T>
struct ObjBlockPtr : BlockPtr {
  GEN_BLOCKPTR_SUPPORT(ObjBlockPtr, BlockPtr) {
    ASSERT0(ptr->size() == sizeof(T));
  }

  T *      object() { reinterpret_cast<T *>(data()); }
  const T *object() const { return reinterpret_cast<const T *>(data()); }

  T *operator->() { return object(); }
  const T *operator->() const { return object(); }

  T &operator*() { return *object(); }
  const T &operator*() const { return *object(); }
};

template <typename T>
struct ObjBlockPtr<T[]> : BlockPtr {
  GEN_BLOCKPTR_SUPPORT(ObjBlockPtr, BlockPtr) {
    ASSERT0(ptr->size() % sizeof(T) == 0);
  }

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
  GEN_BLOCKPTR_SUPPORT(HeadedBlockPtr, BlockPtr) {
    ASSERT0(ptr->size() >= sizeof(THeader));
    ASSERT0((ptr->size() - sizeof(THeader)) % sizeof(TEntry) == 0);
  }

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
