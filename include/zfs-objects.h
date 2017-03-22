#pragma once

#include <cstdio>
#include <memory>

#include "common.h"

#define UB_MAGIC 0x00BAB10C

#define SECTOR_SHIFT 9
#define SECTOR_SIZE (1uL << SECTOR_SHIFT)
#define SECTOR_TO_ADDR(x) (((x) << SECTOR_SHIFT) + (MB * 4))

// Apparently, ZFS on Linux stores 1 less value in the lsize and psize fields
// than the actual value. They call this 'bias', we just call it 'bullshit'.
#define BLKPTR_SIZE_BIAS 1
#define BLKPTR_SIZE_SHIFT SECTOR_SHIFT

// Each class representing a ZFS on-disk object has to:
// - be POD
// - be trivially default-constructible
// - be trivially copiable
// - be marked with __attribute__((packed))
// - contain a VALID_IF() expression with a pure (side-effect free) boolean
// expression

#define VALID_IF(EXPR)                                                   \
  static constexpr const char *const validation_expr = #EXPR;            \
  bool                               validate() const { return (EXPR); } \
  void dump(std::FILE *of) const;

struct Dva {
  u32 asize : 24;
  u8  grid;
  u32 vdev;

  u64  offset : 63;
  bool gang_block : 1;

  size_t getAddress() const { return SECTOR_TO_ADDR(offset); }

  size_t getAllocatedSize() const {
    return static_cast<size_t>(asize) << SECTOR_SHIFT;
  }

  VALID_IF(asize > 0 && offset > 0);
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

enum class BlkptrType : u8 {
  DNode  = 0x0a,
  ObjSet = 0x0b,
};

enum class Endian : bool { Little = 1, Big = 0 };

// See include/sys/zio.h for a full list of compressions that ZFS-on-Linux
// supports. The ZFS on-disk documentation here is not valid.
enum class Compress : u8 {
  Inherit = 0,
  On,
  Off,
  LZ4 = 15,

  Default = LZ4
};

struct Blkptr {
  Dva vdev[3];

  // -- props --
  u16      lsize;
  u16      psize;
  Compress comp : 7;
  bool     embedded : 1;

  u8         cksum;
  BlkptrType type;
  u8         lvl : 5;
  bool       encrypt : 1;
  bool       dedup : 1;
  Endian     endian : 1;
  // -- end props --

  PADDING(3 * sizeof(u64));
  u64 birth_txg;
  u64 fill;
  u8  checksum[32];

  size_t getLogicalSize() const {
    return (static_cast<size_t>(lsize) + BLKPTR_SIZE_BIAS) << BLKPTR_SIZE_SHIFT;
  }

  size_t getPhysicalSize() const {
    return (static_cast<size_t>(psize) + BLKPTR_SIZE_BIAS) << BLKPTR_SIZE_SHIFT;
  }

  VALID_IF(true);
} __attribute__((packed));

static_assert(sizeof(Blkptr) == 128, "Blkptr definition incorrect!");

struct Uberblock {
  u64 magic;
  u64 spa_version; // with ZFS-on-Linux, this is not gonna be 0x1, but some kind
                   // of SPA version tag
  u64    txg;      // transaction group number
  u64    guid_sum;
  u64    timestamp;
  Blkptr rootbp;

  VALID_IF(magic == UB_MAGIC);
} __attribute__((packed));

enum class DNodeType : u8 { Invalid };

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
  Blkptr bps[3];
  PADDING(64);

  VALID_IF(type != DNodeType::Invalid && nblkptr != 0 && nblkptr <= 3);
} __attribute__((packed));

static_assert(sizeof(DNode) == 512, "DNode!");

struct ObjSet {
  DNode metadnode;
  PADDING(8 * sizeof(u64)); // TODO: zil_header
  u64 type;
  PADDING(376);

  VALID_IF(true);
} __attribute__((packed));
