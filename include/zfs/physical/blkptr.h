#pragma once

#include "zfs/general.h"

// Apparently, ZFS on Linux stores 1 less value in the lsize and psize fields
// than the actual value. They call this 'bias', we just call it 'bullshit'.
#define BLKPTR_SIZE_BIAS 1u
#define BLKPTR_SIZE_SHIFT SECTOR_SHIFT

namespace zfs {
namespace physical {

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

  VALID_IF(asize != 0);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

struct Blkptr {
  Dva dva[3];

  // -- props --
  u16      lsize;
  u16      psize;
  Compress comp : 7;
  bool     embedded : 1;

  u8        cksum;
  DNodeType type; // this type seems to be the same as DNode's type
  u8        lvl : 5;
  bool      encrypt : 1;
  bool      dedup : 1;
  Endian    endian : 1;
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

  VALID_IF(type != DNodeType::Invalid);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

#define BLKPTR_SHIFT 7

static_assert(sizeof(Blkptr) == (1 << BLKPTR_SHIFT),
              "Blkptr definition incorrect!");

} // end namespace physical
} // end namespace zfs
