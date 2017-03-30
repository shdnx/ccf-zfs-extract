#pragma once

#include <vector>

#include "zfs/block.h"
#include "zfs/general.h"
#include "zfs/physical/dnode.h"
#include "zfs/zpool_reader.h"

namespace zfs {

using objectid_t = std::size_t;

struct IndirectBlockIterator {}; // TODO

namespace detail {

struct IndirectBlockNode {
  const physical::Blkptr *       blkptr;
  std::size_t                    level;
  std::vector<IndirectBlockNode> children;

  explicit IndirectBlockNode(const physical::Blkptr &blkptr_,
                             std::size_t             level_)
      : blkptr{&blkptr_}, level{level_} {}

  physical::Blkptr getBlkptr(objectid_t id, std::size_t level);
};

} // end namespace detail

struct IndirectBlock {
  using iterator = IndirectBlockIterator;

  explicit IndirectBlock(ZPoolReader &reader, const physical::DNode &root)
      : m_reader{&reader}, m_root{root, nlevels}, m_dnode{&root} {}

  ZPoolReader &reader() { return *m_reader; }
  std::size_t  numLevels() const { return m_root.level; }
  std::size_t  blockSize() const { return 1uL << m_dnode->indblkshift; }
  const physical::DNode &root() const { return *m_dnode; }

  BlockRef objectByID(objectid_t id, std::size_t level = 0);
  BlockCRef objectByID(objectid_t id, std::size_t level = 0) const;

  std::size_t numObjects() const;

  iterator begin();
  iterator end();

private:
  std::size_t _nblkptrPerLevel() const {
    return blockSize() / sizeof(physical::Blkptr);
  }

  ZPoolReader *             m_reader;
  const physical::DNode *   m_dnode;
  detail::IndirectBlockNode m_roots[3];
};

BlockRef IndirectBlock::objectByID(objectid_t id, std::size_t level) {}

} // end namespace zfs
