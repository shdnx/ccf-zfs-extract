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

// lives in the bonus array of ZPL DNode-s
// this seems to be deprecated in ZFS-on-Linux (deprecated in ZPL version
// 5)
/*struct ZNode {
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
} __attribute__((packed));*/

// this seems to be the new "ZNode" from ZPL v5
// this structure doesn't exist directly in ZFS, they switched to a "system
// attribute" (SA) management system - this order and size of fields was
// determined based on dump_znode() in zdb.c and zfs_sa.h's SA_*_OFFSET macros,
// as well as manual examination of the data (e.g. the timestamps can be
// recognised)
// there are more fields (see enum zpl_attr), but the order of them I'm not sure
// about
struct ZNode {
  PADDING(8); // header?
  u64       mode;
  u64       size;
  u64       gen;
  u64       uid;
  u64       gid;
  u64       parent;
  u64       links;  // ?
  ZNodeTime atime;  // accessed
  ZNodeTime mtime;  // modified
  ZNodeTime crtime; // created
  ZNodeTime ctime;  // changed
  u64       flags;
  VALID_IF(true);

  void dump(std::FILE *of, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

} // end namespace physical
} // end namespace zfs
