#pragma once

#include "zfs/general.h"
#include "zfs/physical/blkptr.h"

#define UB_MAGIC 0x00BAB10C

namespace zfs {
namespace physical {

struct Uberblock {
  u64 magic;
  u64 spa_version; // with ZFS-on-Linux, this is not gonna be 0x1, but some kind
                   // of SPA version tag
  u64    txg;      // transaction group number
  u64    guid_sum;
  u64    timestamp;
  Blkptr rootbp;

  VALID_IF(magic == UB_MAGIC);
  void dump(FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

} // end namespace physical
} // end namespace zfs
