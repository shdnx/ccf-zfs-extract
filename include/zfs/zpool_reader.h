#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "utils/common.h"
#include "zfs/physical/blkptr.h"

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

  bool readRawData(const physical::Blkptr &bp, u32 dva_index, OUT void *data);

  bool readRawData(const physical::Blkptr &bp, u32 dva_index,
                   OUT std::unique_ptr<char[]> *pdata) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    std::unique_ptr<char[]> buffer{new char[bp.getLogicalSize()]};
    ASSERT(buffer, "Buffer allocation failed for size: %zu",
           bp.getLogicalSize());

    if (!readRawData(bp, dva_index, OUT static_cast<void *>(buffer.get())))
      return false;

    *pdata = std::move(buffer);
    return true;
  }

  template <typename TObj>
  bool readObject(const physical::Blkptr &bp, u32 dva_index, OUT TObj *obj) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");
    ASSERT(bp.getLogicalSize() == sizeof(TObj),
           "Blkptr refers to a block of logical size %zu, whereas obj size is "
           "%zu: not a single object!",
           bp.getLogicalSize(), sizeof(TObj));

    return readRawData(bp, dva_index, OUT obj);
  }

  // variable length objects
  template <typename TObj>
  bool readVLObject(const physical::Blkptr &bp, u32 dva_index, OUT TObj *obj) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");
    return readRawData(bp, dva_index, OUT obj);
  }

  template <typename TObj>
  size_t readObjectArray(const physical::Blkptr &bp, u32 dva_index,
                         OUT TObj *objs) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    const size_t lsize = bp.getLogicalSize();
    ASSERT(lsize % sizeof(TObj) == 0, "Cannot read array of objects of size "
                                      "%zu from a block with logical size %zu!",
           sizeof(TObj), lsize);

    if (!readRawData(bp, dva_index, OUT objs))
      return 0;

    return lsize / sizeof(TObj);
  }

  template <typename TObj>
  size_t readObjectArray(const physical::Blkptr &bp, u32 dva_index,
                         OUT std::unique_ptr<TObj[]> *pobjs) {
    ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");

    const size_t lsize = bp.getLogicalSize();
    ASSERT(lsize % sizeof(TObj) == 0, "Cannot read array of objects of size "
                                      "%zu from a block with logical size %zu!",
           sizeof(TObj), lsize);

    if (!readRawData(bp, dva_index,
                     OUT reinterpret_cast<std::unique_ptr<char[]> *>(pobjs)))
      return 0;

    return lsize / sizeof(TObj);
  }

private:
  std::FILE *m_fp;
  bool       m_own;
};

} // end namespace zfs
