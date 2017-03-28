#pragma once

#include <cstdio>

#include "utils/common.h"

#define SECTOR_SHIFT 9
#define SECTOR_SIZE (1uL << SECTOR_SHIFT)
#define SECTOR_TO_ADDR(x) (((x) << SECTOR_SHIFT) + (MB * 4))

namespace zfs {

// matches dmu_object_type in dmu.h in ZFS-on-Linux
enum class DNodeType : u8 {
  Invalid,
  ObjDirectory = 1, // contains information about meta objects
  DNode        = 10,
  ObjSet       = 11,
  DataSet      = 16,
  FileContents = 19,
  DirContents  = 20,
  MasterNode   = 21,
};

static inline const char *getDNodeTypeAsString(DNodeType dt) {
#define DT(NAME)        \
  case DNodeType::NAME: \
    return #NAME

  switch (dt) {
    DT(Invalid);
    DT(ObjDirectory);
    DT(DNode);
    DT(ObjSet);
    DT(DataSet);
    DT(FileContents);
    DT(DirContents);
    DT(MasterNode);

  default:
    return "(unknown)";
  }

#undef DT
}

// See include/sys/zio.h for a full list of compressions that ZFS-on-Linux
// supports. The ZFS on-disk documentation is not correct here.
enum class Compress : u8 {
  Inherit = 0,
  On,
  Off,
  LZ4 = 0xf,

  Default = LZ4
};

// Each class representing a ZFS on-disk object has to:
// - be POD
// - be trivially default-constructible
// - be trivially copiable
// - be marked with __attribute__((packed))
// - contain a VALID_IF() expression with a pure (side-effect free) boolean
// expression

enum class DumpFlags { None, AllowInvalid = 1 };

// unfortunately, we cannot use inheritance, because these data structures need
// to be directly readable from the disk for simplicity
#define VALID_IF(EXPR)                                        \
  static constexpr const char *const validation_expr = #EXPR; \
  bool                               isValid() const { return (EXPR); }

} // end namespace zfs
