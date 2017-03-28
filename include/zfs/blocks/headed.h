#pragma once

#include <memory>

#include "utils/ptr_range.h"
#include "zfs/general.h"

namespace zfs {

template <typename THeader, typename TEntry>
struct HeadedBlock {
  using entry_range = ConstPtrRange<TEntry>;

  explicit HeadedBlock(size_t size)
      : m_size{size}, m_data{new char[size]},
        m_entries{
            reinterpret_cast<const TEntry *>(m_data.get() + sizeof(THeader)),
            reinterpret_cast<const TEntry *>(m_data.get() + size)} {
    ASSERT(size >= sizeof(THeader),
           "Block header size (%zu) is less than total block size (%zu)!",
           sizeof(THeader), size);
    ASSERT((size - sizeof(THeader)) % sizeof(TEntry) == 0,
           "Misaligned header or entries: block size = %zu, header size = %zu, "
           "entry size = %zu",
           size, sizeof(THeader), sizeof(TEntry));
  }

  HeadedBlock(const HeadedBlock &) = delete;

  HeadedBlock(HeadedBlock &&rhs)
      : m_size{rhs.m_size}, m_data{std::move(rhs.m_data)},
        m_entries{std::move(rhs.m_entries)} {}

  HeadedBlock &operator=(const HeadedBlock &rhs) = delete;

  HeadedBlock &operator=(HeadedBlock &&rhs) {
    m_size    = rhs.m_size;
    m_data    = std::move(rhs.m_data);
    m_entries = std::move(rhs.m_entries);
    return *this;
  }

  void *      data() { return m_data.get(); }
  const void *data() const { return m_data.get(); }

  size_t size() const { return m_size; }

  THeader *header() { return reinterpret_cast<THeader *>(data()); }

  const THeader *header() const {
    return reinterpret_cast<const THeader *>(data());
  }

  const THeader *operator->() const { return header(); }

  const entry_range &entries() const { return m_entries; }
  size_t             numEntries() const { return entries().size(); }

  VALID_IF(header() && header()->isValid());

private:
  size_t                  m_size;
  std::unique_ptr<char[]> m_data;
  entry_range             m_entries;
};

} // end namespace zfs
