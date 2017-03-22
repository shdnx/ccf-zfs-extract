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

  template <typename TObj>
  bool readObject(const Blkptr &bp, u32 vdev_index, OUT TObj *obj) {
    if (!_readObjectImpl(bp, vdev_index, sizeof(TObj), obj))
      return false;

    // TODO: currently commented out, because it cannot be trusted
    // return obj->validate();
    return true;
  }

private:
  bool _readObjectImpl(const Blkptr &bp, u32 vdev_index, size_t obj_size,
                       OUT void *obj);

  std::FILE *m_fp;
  bool       m_own;
};
