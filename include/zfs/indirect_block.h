#pragma once

#include <array>
#include <vector>

#include "utils/iterator_range.h"

#include "zfs/block.h"
#include "zfs/general.h"
#include "zfs/physical/blkptr.h"
#include "zfs/physical/dnode.h"
#include "zfs/zpool_reader.h"

namespace zfs {
namespace detail {

struct IndirectBlockNode {
  IndirectBlockNode() : m_blkptr{nullptr} {}

  void init(const physical::Blkptr &blkptr, std::size_t level) {
    m_blkptr = &blkptr;
    m_level  = level;
  }

  std::size_t size() const { return m_blkptr->getLogicalSize(); }

  BlockRef readBlock(ZPoolReader &reader, bool allowRead);

  IndirectBlockNode *readIndirectChild(ZPoolReader &reader, std::size_t index,
                                       bool allowRead);

private:
  const physical::Blkptr *       m_blkptr;
  BlockPtr                       m_ptr;
  std::size_t                    m_level;
  std::vector<IndirectBlockNode> m_children;
};

struct IndirectBlockBase {
  explicit IndirectBlockBase(ZPoolReader &reader, const physical::DNode &root)
      : m_reader{&reader}, m_dnode{&root} {
    for (std::size_t i = 0; i < root.nblkptr; i++) {
      m_roots[i].init(root.bps[i], numLevels());
    }
  }

  ZPoolReader &          reader() { return *m_reader; }
  const physical::DNode &root() const { return *m_dnode; }

  std::size_t numLevels() const { return m_dnode->nlevels; }
  std::size_t indirectBlockSize() const { return 1uL << m_dnode->indblkshift; }
  std::size_t dataBlockSize() const {
    return m_dnode->data_sectors << SECTOR_SHIFT;
  }
  std::size_t numDataBlocks() const { return m_dnode->max_block_id + 1; }

  // Total data size represented by this indirect block.
  // Note that this is usually not exactly precise, only gives the file size on
  // a data block size granularity. Some metadata somewhere else likely stores
  // the exact size.
  std::size_t size() const { return numDataBlocks() * dataBlockSize(); }

  // The total number of data blocks this indirect block could maximum
  // reference.
  std::size_t totalDataBlocks() const {
    return 1 << ((m_dnode->indblkshift - BLKPTR_SHIFT) * numLevels());
  }

protected:
  BlockRef blockByIDImpl(u64 blockid) {
    return _getChildNode(blockid, true)->readBlock(*m_reader, true);
  }

  BlockPtr *blockByIDImpl(u64 blockid) const {
    detail::IndirectBlockNode *node =
        const_cast<IndirectBlockBase *>(this)->_getChildNode(blockid, false);
    if (!node)
      return nullptr;

    return &node->readBlock(*m_reader, false);
  }

private:
  // Calculate index into an indirect block on the given level that will contain
  // the block of the given ID.
  std::size_t _calculateIndex(u64 blockid, int level) const {
    const std::size_t shift = m_dnode->indblkshift - BLKPTR_SHIFT;
    const std::size_t mask  = (1 << shift) - 1;
    return (blockid >> (shift * level)) & mask;
  }

  detail::IndirectBlockNode *_getChildNode(u64 blockid, bool allowRead);

  ZPoolReader *          m_reader;
  const physical::DNode *m_dnode;
  std::array<detail::IndirectBlockNode, 3> m_roots;
};

template <typename T>
struct IndirectBlockBlockIterator {
  explicit IndirectBlockBlockIterator(T &block, u64 blockid)
      : m_block{&block}, m_blockid{blockid} {}

  auto &operator*() { return m_block->blockByID(m_blockid); }

  IndirectBlockBlockIterator &operator++() {
    m_blockid++;
    return *this;
  }

  IndirectBlockBlockIterator operator++(int) {
    IndirectBlockBlockIterator copy{*this};
    operator++();
    return copy;
  }

  bool operator==(const IndirectBlockBlockIterator &rhs) const {
    return m_block == rhs.m_block && m_blockid == rhs.m_blockid;
  }

  bool operator!=(const IndirectBlockBlockIterator &rhs) const {
    return !operator==(rhs);
  }

private:
  T * m_block;
  u64 m_blockid;
};

} // end namespace detail

struct IndirectBlock : detail::IndirectBlockBase {
  using block_iterator = detail::IndirectBlockBlockIterator<IndirectBlock>;
  using iterator       = block_iterator;

  explicit IndirectBlock(ZPoolReader &reader, const physical::DNode &root)
      : detail::IndirectBlockBase{reader, root} {}

  BlockRef blockByID(u64 blockid) { return blockByIDImpl(blockid); }
  BlockPtr *blockByID(u64 blockid) const { return blockByIDImpl(blockid); }

  block_iterator block_begin() { return block_iterator{*this, 0}; }
  block_iterator block_end() { return block_iterator{*this, numDataBlocks()}; }

  IteratorRange<block_iterator> blocks() {
    return {block_begin(), block_end()};
  }

  iterator begin() { return block_begin(); }
  iterator end() { return block_end(); }

  // TODO: we could also have a const_iterator that skips over the unread parts,
  // but... it's messy
};

// Represents an indirect block where the leaf blocks are arrays of TObj
// objects.
template <typename TObj>
struct IndirectObjBlock : detail::IndirectBlockBase {
  using block_iterator = detail::IndirectBlockBlockIterator<IndirectObjBlock>;

  struct object_iterator {
    using reference = TObj &;

    explicit object_iterator(IndirectObjBlock &block, u64 objid)
        : m_block{&block}, m_objid{objid} {}

    reference operator*() { return m_block->objectByID(m_objid); }

    object_iterator &operator++() {
      m_objid++;
      return *this;
    }

    object_iterator operator++(int) {
      object_iterator copy{*this};
      operator++();
      return copy;
    }

    bool operator==(const object_iterator &rhs) const {
      return m_block == rhs.m_block && m_objid == rhs.m_objid;
    }

    bool operator!=(const object_iterator &rhs) const {
      return !operator==(rhs);
    }

  private:
    IndirectObjBlock *m_block;
    u64               m_objid;
  };

  using iterator = object_iterator;

  explicit IndirectObjBlock(ZPoolReader &reader, const physical::DNode &root)
      : detail::IndirectBlockBase{reader, root} {}

  std::size_t numObjects() const { return size() / sizeof(TObj); }

  std::size_t numObjectsPerBlock() const {
    return numObjects() / numDataBlocks();
  }

  ArrayBlockRef<TObj> blockByID(u64 blockid) {
    // no idea why GCC needs the 'template' before the cast() call...
    return blockByIDImpl(blockid).template cast<ArrayBlockRef<TObj>>();
  }

  ArrayBlockPtr<TObj> *blockByID(u64 blockid) const {
    return static_cast<ArrayBlockPtr<TObj> *>(blockByIDImpl(blockid));
  }

  TObj &objectByID(u64 objid) {
    ASSERT0(objid < numObjects());
    const std::size_t   objsPerBlock = numObjectsPerBlock();
    ArrayBlockRef<TObj> objArray     = blockByID(objid / objsPerBlock);
    return objArray[objid % objsPerBlock];
  }

  const TObj *objectByID(u64 objid) const {
    ASSERT0(objid < numObjects());
    const std::size_t objsPerBlock = numObjectsPerBlock();

    ArrayBlockPtr<TObj> *objArray;
    if (!blockByID(objid / objsPerBlock, OUT & objArray))
      return nullptr;

    return &(*objArray)[objid % objsPerBlock];
  }

  block_iterator block_begin() { return block_iterator{*this, 0}; }
  block_iterator block_end() { return block_iterator{*this, numDataBlocks()}; }

  IteratorRange<block_iterator> blocks() {
    return {block_begin(), block_end()};
  }

  object_iterator object_begin() { return object_iterator{*this, 0}; }
  object_iterator object_end() { return object_iterator{*this, numObjects()}; }

  IteratorRange<object_iterator> objects() {
    return {object_begin(), object_end()};
  }

  iterator begin() { return object_begin(); }
  iterator end() { return object_end(); }
};

} // end namespace zfs
