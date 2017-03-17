#pragma once

#include <memory>

#include "common.h"

#define UB_MAGIC 0x00BAB10C

struct Dva {
  u32 asize : 24;
  u8  grid;
  u32 vdev;

  u64  offset : 63;
  bool gang_block : 1;
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

enum class BlkptrType : u8 {
  DNode  = 0x0a,
  ObjSet = 0x0b,
};

enum class Endianness : bool { Little = 1, Big = 0 };

struct BlkptrProps {
  u16  lsize;
  u16  psize;
  u8   comp : 7;
  bool embedded : 1;

  u8         cksum;
  BlkptrType type;
  u8         lvl : 5;
  bool       encrypt : 1;
  bool       dedup : 1;
  Endianness endian : 1;
} __attribute__((packed));

static_assert(sizeof(BlkptrProps) == 8, "Props definition incorrect!");

struct Blkptr {
  Dva         vdev[3];
  BlkptrProps props;

  PADDING(3 * sizeof(u64));
  u64 birth_txg;
  u64 fill;
  u8  checksum[32];
} __attribute__((packed));

static_assert(sizeof(Blkptr) == 128, "Blkptr definition incorrect!");

struct Uberblock {
  u64    magic;
  u64    version;
  u64    txg;
  u64    guid_sum;
  u64    timestamp;
  Blkptr rootbp;

  static std::unique_ptr<Uberblock> read(FILE *fp);
  bool readFrom(FILE *fp);
} __attribute__((packed));

enum class DNodeType : u8 {

};

struct DNode {
  /*u8        pad[1];
  u8        phys_comp; // physical compress flag?
  u8        checksum;
  u8        bonustype;
  u8        nblkptr; // number of block pointers
  u8        nlevels; // number of levels of indirection
  u8        indblkshift;
  DNodeType type;*/
  DNodeType type;
  u8        indblkshift;
  u8        nlevels; // number of levels of indirection
  u8        nblkptr; // number of block pointers
  u8        bonustype;
  u8        checksum;
  u8        phys_comp; // physical compress flag?
  u8        flags;
  // u8        pad[1];    // flags??

  u16 data_blk_size_secs;
  u16 bonuslen;
  PADDING(4);

  /*u8  pad2[3];
  u8  extra_slots;
  u16 bonuslen;
  u16 flags;*/

  u64 max_block_id;
  u64 secphys_used;
  PADDING(4 * sizeof(u64));

  // NOTE: this is not actually the full story, there are also bonus arrays, and
  // so on
  Blkptr bps[3];
  PADDING(64);
} __attribute__((packed));

static_assert(sizeof(DNode) == 512, "DNode!");

//#define SECTOR_TO_ADDR(x) ((x) << 9)
#define SECTOR_TO_ADDR(x) (((x) << 9) + (4 * MB))