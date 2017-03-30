#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "utils/common.h"
#include "zfs/block.h"
#include "zfs/physical/blkptr.h"
#include "zfs/physical/uberblock.h"

#define VDEV_NLABELS 4
#define VDEV_LABEL_NUBERBLOCKS 128

namespace zfs {

struct ZPoolReader {
  static std::unique_ptr<ZPoolReader> open(const std::string &path) {
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp)
      return nullptr;

    return std::make_unique<ZPoolReader>(fp, true);
  }

  explicit ZPoolReader(std::FILE *fp, bool own = false)
      : m_fp{fp}, m_own{own} {}

  ZPoolReader(const ZPoolReader &other) = delete;

  ZPoolReader(ZPoolReader &&other) : m_fp{other.m_fp}, m_own{other.m_own} {
    other.m_fp  = nullptr;
    other.m_own = false;
  }

  ~ZPoolReader() {
    if (m_own && m_fp)
      std::fclose(m_fp);
  }

  // label_index < VDEV_NLABELS, ub_index <= VDEV_LABEL_NUBERBLOCKS
  bool readUberblock(u32 label_index, u32 ub_index,
                     OUT physical::Uberblock *ub);

  bool read(const physical::Blkptr &bp, u32 dva_index, OUT void *data);

  BlockPtr read(const physical::Blkptr &pbp, u32 dva_index) {
    ASSERT(pbp.isValid(), "Cannot resolve invalid blkptr!");

    BlockPtr bp = BlockPtr::allocate(pbp.getLogicalSize());
    if (!bp || !read(pbp, dva_index, OUT bp.data()))
      return nullptr;

    return std::move(bp);
  }

  template <typename TPtr>
  TPtr read(const physical::Blkptr &pbp, u32 dva_index) {
    return read(pbp, dva_index).cast<TPtr>();
  }

  template <typename TPtr>
  bool read(const physical::Blkptr &pbp, u32 dva_index, OUT TPtr *bp) {
    *bp = read<TPtr>(pbp, dva_index);
    return *bp;
  }

private:
  std::FILE *m_fp;
  bool       m_own;
};

} // end namespace zfs
