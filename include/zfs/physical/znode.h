#pragma once

#include "zfs/general.h"

namespace zfs {
namespace physical {

struct ZNodeTime {
  u64 seconds;
  u64 nanoseconds;

  VALID_IF(true);
  void dump(std::FILE *of, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

// lives in the bonus array of ZPS DNode-s
// NOTE: this seems to be deprecated in ZFS-on-Linux (deprecated in ZPL version
// 5)
struct ZNode {
  ZNodeTime time_accessed;
  ZNodeTime time_modified;
  ZNodeTime time_changed;
  ZNodeTime time_created;
  u64       gen_txg;
  u64       mode;
  u64       size;
  u64       parent_obj;
  u64       links;
  u64       xattr;
  u64       rdev;
  u64       flags;
  u64       uid;
  u64       gid;
  PADDING(4 * sizeof(u64));
  PADDING(2 * sizeof(u64) + 2 * sizeof(u64) * 6); // zp_acl

  VALID_IF(true);

  void dump(std::FILE *of, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

} // end namespace physical
} // end namespace zfs
