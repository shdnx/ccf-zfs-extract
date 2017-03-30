#pragma once

#include "common.h"

// replace with C++17 std::array_view<T>
template <typename T>
struct ArrayView;

template <typename T>
struct ArrayView<T> {
  using iterator        = T *;
  using const_iterator  = const T *;
  using reference       = T &;
  using const_reference = const T &;

  explicit ArrayView(T *data, size_t size) : m_data{data}, m_size{size} {}

  explicit ArrayView(void *data, size_t dataSize)
      : ArrayView{static_cast<T *>(data), dataSize / sizeof(T)} {
    ASSERT0(dataSize % sizeof(T) == 0);
  }

  size_t size() const { return m_size; }

  T *      data() { return m_data; }
  const T *data() const { return m_data; }

  T &operator[](size_t index) { return m_data[index]; }
  const T &operator[](size_t index) const { return m_data[index]; }

  iterator       begin() { return m_data; }
  const_iterator begin() const { return m_data; }

  iterator       end() { return m_data + m_size; }
  const_iterator end() const { return m_data + m_size; }

  ArrayView<T> slice(size_t startIndex) const {
    ASSERT0(startIndex < m_size);
    return ArrayView<T>{m_data + startIndex, m_size - startIndex};
  }

  ArrayView<T> slice(size_t startIndex, size_t length) const {
    ASSERT0(startIndex < m_size);
    ASSERT0(startIndex + length < m_size);
    return ArrayView<T>{m_data + startIndex, length};
  }

private:
  T *    m_data;
  size_t m_size;
};
/*
template<typename T>
struct ArrayView<const T> {
  using iterator = const T*;
  using const_iterator = iterator;
  using reference = const T&;
  using const_reference = reference;

  explicit ArrayView(const T *data, size_t size) : m_data{data}, m_size{size} {}

  explicit ArrayView(const void *data, size_t dataSize) :
ArrayView{static_cast<const T*>(data), dataSize / sizeof(T)} {
    ASSERT0(dataSize % sizeof(T) == 0);
  }

  size_t size() const { return m_size; }
  const T *data() const { return m_data; }

  const T &operator[](size_t index) const { return m_data[index]; }

  const_iterator begin() const { return m_data; }
  const_iterator end() const { return m_data + m_size; }

  ArrayView<T> slice(size_t startIndex) const {
    ASSERT0(startIndex < m_size);
    return ArrayView<T>{m_data + startIndex, m_size - startIndex};
  }

  ArrayView<T> slice(size_t startIndex, size_t length) const {
    ASSERT0(startIndex < m_size);
    ASSERT0(startIndex + length < m_size);
    return ArrayView<T>{m_data + startIndex, length};
  }

private:
  const T *    m_data;
  size_t m_size;
};
*/