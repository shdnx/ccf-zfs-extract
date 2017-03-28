#pragma once

#include <memory>
#include <vector>

#include "utils/array_view.h"

#include "zfs/blocks/data.h"
#include "zfs/general.h"
#include "zfs/physical/blkptr.h"

namespace zfs {

struct IndirectBlock : DataBlock {
  explicit IndirectBlock(size_t size, size_t level)
      : DataBlock{size}, m_level{level} {
    ASSERT0(level > 0);
    ASSERT0(m_size % sizeof(Blkptr) == 0);
  }

  size_t numChildren() const { return size() / sizeof(Blkptr); }

  DataBlock *child(size_t index) {
    ASSERT0(m_children.empty() || index < m_children.size());

    if (!m_children.empty() && m_children[index])
      return m_children[index];

    return nullptr;
  }

  DataBlock *readChild(ZPool &zpool, size_t index, size_t dva = 0) {
    if (DataBlock *result = child(index))
      return result;

    const Blkptr &bp = dataAsArray<Blkptr>()[index];
    if (!bp.isValid())
      return nullptr;

    std::unique_ptr<DataBlock> node;
    if (m_level == 1)
      node = std::make_unique(bp.getLogicalSize());
    else
      node = std::make_unique<IndirectBlock>(bp.getLogicalSize(), m_level - 1);

    if (!node)
      return nullptr;

    if (!zpool.readRawData(bp, dva, OUT & node->data()))
      return nullptr;

    if (m_children.empty())
      m_children.resize(numChildren());

    m_children[index] = std::move(node);
    return m_children[index].get();
  }

private:
  size_t                                  m_level;
  std::vector<std::unique_ptr<DataBlock>> m_children;
};

} // end namespace zfs
