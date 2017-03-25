#pragma once

#include <cstdint>

template <typename T>
struct ConstPtrIterator {
  using reference = const T &;
  using pointer   = const T *;

  explicit ConstPtrIterator(pointer entry) : m_entry{entry} {}

  reference operator*() const { return *m_entry; }
  pointer operator->() const { return m_entry; }

  ConstPtrIterator &operator++() {
    m_entry++;
    return *this;
  }

  ConstPtrIterator operator++(int) {
    ConstPtrIterator old{*this};
    m_entry++;
    return old;
  }

  bool operator==(const ConstPtrIterator &rhs) const {
    return m_entry == rhs.m_entry;
  }

  bool operator!=(const ConstPtrIterator &rhs) const {
    return !operator==(rhs);
  }

private:
  pointer m_entry;
};

template <typename T>
struct ConstPtrRange {
  using iterator       = ConstPtrIterator<T>;
  using item_pointer   = typename iterator::pointer;
  using item_reference = typename iterator::reference;

  explicit ConstPtrRange(item_pointer begin, item_pointer end)
      : m_begin{begin}, m_end{end} {}

  size_t size() const { return m_end - m_begin; }

  item_reference operator[](size_t i) const { return m_begin[i]; }

  iterator begin() const { return iterator{m_begin}; }
  iterator end() const { return iterator{m_end}; }

private:
  item_pointer m_begin;
  item_pointer m_end;
};
