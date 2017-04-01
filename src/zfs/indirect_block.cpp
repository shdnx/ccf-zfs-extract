#include "utils/log.h"

#include "zfs/indirect_block.h"

namespace zfs {

namespace detail {

BlockRef IndirectBlockNode::readBlock(ZPoolReader &reader, bool allowRead) {
  ASSERT0(m_blkptr);

  if (!m_ptr && allowRead) {
    m_ptr = reader.read(*m_blkptr, /*dva=*/0);
    ASSERT0(m_ptr); // TODO: throw an exception
  }

  return m_ptr;
}

IndirectBlockNode *IndirectBlockNode::readIndirectChild(ZPoolReader &reader,
                                                        std::size_t  index,
                                                        bool allowRead) {
  ASSERT0(m_level > 0);

  if (m_children.empty()) {
    auto &arr =
        readBlock(reader, allowRead).cast<ArrayBlockRef<physical::Blkptr>>();
    if (!arr)
      return nullptr;

    m_children.resize(arr.numObjects());

    for (std::size_t i = 0; i < arr.numObjects(); i++) {
      m_children[i].init(arr[i], m_level - 1);
    }
  }

  return &m_children[index];
}

detail::IndirectBlockNode *IndirectBlockBase::_getChildNode(u64  blockid,
                                                            bool allowRead) {
  ASSERT0(blockid < numDataBlocks());

  // first have to choose between the DNode's own blkptrs
  // TODO: how?????
  // const std::size_t          blocksPerRoot = numDataBlocks() /
  // m_roots.size();
  // const std::size_t          rootIndex     = blockid / blocksPerRoot;
  const std::size_t          rootIndex = 0;
  detail::IndirectBlockNode *node      = &m_roots[rootIndex];

  // then come the indirect block levels, which are just arrays of blkptrs
  // for (int level = numLevels() - 1; level > 0; level--) {
  for (int level = numLevels() - 2; level >= 0; level--) {
    ASSERT0(node->size() == indirectBlockSize());

    const std::size_t index = _calculateIndex(blockid, level);

    node = node->readIndirectChild(*m_reader, index, true);
    if (!node)
      return nullptr;
  }

  return node;
}

} // end namespace detail
} // end namespace zfs
