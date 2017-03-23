#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "common.h"
#include "zfs-objects.h"

#define VDEV_NLABELS 4
#define VDEV_LABEL_NUBERBLOCKS 128

class ZPool {
public:
  static std::unique_ptr<ZPool> open(const std::string &path) {
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp)
      return nullptr;

    return std::make_unique<ZPool>(fp, true);
  }

  explicit ZPool(FILE *fp, bool own = false) : m_fp{fp}, m_own{own} {}

  ZPool(const ZPool &other) = delete;

  ZPool(ZPool &&other) : m_fp{other.m_fp}, m_own{other.m_own} {
    other.m_fp  = nullptr;
    other.m_own = false;
  }

  ~ZPool() {
    if (m_own && m_fp)
      std::fclose(m_fp);
  }

  // label_index < VDEV_NLABELS, ub_index <= VDEV_LABEL_NUBERBLOCKS
  bool readUberblock(u32 label_index, u32 ub_index, OUT Uberblock *ub);

  bool readRawData(const Blkptr &bp, u32 vdev_index, OUT void *data);

  bool readRawData(const Blkptr &bp, u32 vdev_index,
                   OUT std::unique_ptr<char[]> *pdata) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    std::unique_ptr<char[]> buffer{new char[bp.getLogicalSize()]};
    ASSERT(buffer, "Buffer allocation failed for size: %zu",
           bp.getLogicalSize());

    if (!readRawData(bp, vdev_index, OUT static_cast<void *>(buffer.get())))
      return false;

    *pdata = std::move(buffer);
    return true;
  }

  template <typename TObj>
  bool readObject(const Blkptr &bp, u32 vdev_index, OUT TObj *obj) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");
    ASSERT(bp.getLogicalSize() == sizeof(TObj),
           "Blkptr refers to a block of logical size %zu, whereas obj size is "
           "%zu: not a single object!",
           bp.getLogicalSize(), sizeof(TObj));

    return readRawData(bp, vdev_index, OUT obj);
  }

  template <typename TObj>
  size_t readObjectArray(const Blkptr &bp, u32 vdev_index, OUT TObj *objs) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    const size_t lsize = bp.getLogicalSize();
    ASSERT(lsize % sizeof(TObj) == 0, "Cannot read array of objects of size "
                                      "%zu from a block with logical size %zu!",
           sizeof(TObj), lsize);

    if (!readRawData(bp, vdev_index, OUT objs))
      return 0;

    return lsize / sizeof(TObj);
  }

  template <typename TObj>
  size_t readObjectArray(const Blkptr &bp, u32 vdev_index,
                         OUT std::unique_ptr<TObj[]> *pobjs) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    const size_t lsize = bp.getLogicalSize();
    ASSERT(lsize % sizeof(TObj) == 0, "Cannot read array of objects of size "
                                      "%zu from a block with logical size %zu!",
           sizeof(TObj), lsize);

    if (!readRawData(bp, vdev_index,
                     OUT reinterpret_cast<std::unique_ptr<char[]> *>(pobjs)))
      return 0;

    return lsize / sizeof(TObj);
  }

private:
  std::FILE *m_fp;
  bool       m_own;
};
