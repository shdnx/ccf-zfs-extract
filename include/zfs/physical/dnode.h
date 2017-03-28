#pragma once

#include "zfs/general.h"
#include "zfs/physical/blkptr.h"

namespace zfs {
namespace physical {

struct DNode {
  DNodeType type;
  u8        indblkshift;
  u8        nlevels; // number of levels of indirection
  u8        nblkptr; // number of block pointers
  u8        bonustype;
  u8        checksum;
  u8        phys_comp; // physical compress flag?
  u8        flags;

  u16 data_blk_size_secs;
  u16 bonuslen;
  PADDING(4);

  u64 max_block_id;
  u64 secphys_used;
  PADDING(4 * sizeof(u64));

  // NOTE: this is not actually the full story, there are also bonus arrays, and
  // so on
  union {
    Blkptr bps[3];

    struct {
      PADDING(sizeof(Blkptr));
      u8 bonus[2 * sizeof(Blkptr) + 64];
    };

    struct {
      PADDING(2 * sizeof(Blkptr) + 64);
      Blkptr spill;
    };
  };

  VALID_IF(type != DNodeType::Invalid && nblkptr != 0 && nblkptr <= 3);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;

  template <typename T>
  const T *getBonusAs() const {
    ASSERT0(bonustype != 0 && bonuslen != 0 && nblkptr < 3);
    return reinterpret_cast<const T *>(bonus);
  }
} __attribute__((packed));

static_assert(sizeof(DNode) == 512, "DNode!");

// -- data structures that live in the DNode bonus region --

struct DSLDir {
  u64 creation_time; // unused by ZFS-on-Linux
  u64 head_dataset_obj;
  u64 parent_obj;
  u64 origin_obj;
  u64 child_dir_zapobj;
  u64 used_bytes;
  u64 compressed_bytes;
  u64 uncompressed_bytes;
  u64 quota;
  u64 reserved;
  u64 props_zapobj;
  u64 deleg_zapobj;
  u64 flags;
  u64 used_breakdown[5]; // TODO: enum dd_used
  u64 clones;
  PADDING(13 * sizeof(u64));

  VALID_IF(true);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

static_assert(sizeof(DSLDir) == 256, "DSLDir definition invalid!");

struct DSLDataSet {
  u64    dir_obj;
  u64    prev_snap_obj;
  u64    prev_snap_txg;
  u64    next_snap_obj;
  u64    snapnames_zapobj;
  u64    nchildren;
  u64    creation_time;
  u64    creation_txg;
  u64    deadlist_obj;
  u64    referenced_bytes;
  u64    compressed_bytes;
  u64    uncompressed_bytes;
  u64    unique_bytes;
  u64    fsid_guid;
  u64    guid;
  u64    flags;
  Blkptr bp;
  u64    next_clones_obj;
  u64    props_obj;
  u64    userrefs_obj;
  PADDING(5 * sizeof(u64));

  VALID_IF(true);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

} // end namespace physical
} // end namespace zfs
