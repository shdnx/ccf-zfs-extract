#pragma once

#include <vector>

#include "zfs/block.h"
#include "zfs/general.h"
#include "zfs/zpool_reader.h"

namespace zfs {

using objectid_t = std::size_t;

struct IndirectBlockIterator {};

namespace detail {

struct IndirectBlockNode {
  BlockRef                       block;
  std::size_t                    level;
  std::vector<IndirectBlockNode> children;

  explicit IndirectBlockNode(BlockRef block_, std::size_t level_)
      : block{block_}, level{level_} {}

  physical::Blkptr getBlkptr(objectid_t id);
};

} // end namespace detail

struct IndirectBlock {
  using iterator = IndirectBlockIterator;

  explicit IndirectBlock(ZPoolReader &reader, BlockRef root,
                         std::size_t nlevels)
      : m_reader{&reader}, m_root{root, nlevels} {}

  ZPoolReader &reader() { return *m_reader; }
  std::size_t  numLevels() const { return m_root.level; }
  BlockRef     root() { return m_root.block; }
  BlockCRef    root() const { return m_root.block; }

  BlockRef objectByID(objectid_t id);
  BlockCRef objectByID(objectid_t id) const;

  std::size_t numObjects() const;

  iterator begin();
  iterator end();

private:
  ZPoolReader *             m_reader;
  detail::IndirectBlockNode m_root;
};

} // end namespace zfs
