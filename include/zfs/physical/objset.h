#pragma once

#include "zfs/general.h"
#include "zfs/physical/dnode.h"

namespace zfs {
namespace physical {

struct ObjSet {
  DNode metadnode;
  PADDING(8 * sizeof(u64)); // TODO: zil_header
  u64 type;
  u64 flags;
  PADDING(432);
  DNode userused_dnode;
  DNode groupused_dnode;

  VALID_IF(true);
  void dump(FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

static_assert(sizeof(ObjSet) == 2048, "ObjSet definition invalid!");

} // end namespace physical
} // end namespace zfs
