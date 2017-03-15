#pragma once

#include <memory>

#include "common.h"

#define UB_MAGIC 0x00BAB10C

struct Dva {
  uint32_t  asize : 24;
  uint8_t   grid;   
  uint32_t  vdev;

  uint64_t  offset : 63;
  bool      gang_block : 1;
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

struct Props {
  uint16_t  lsize;
  uint16_t  psize;
  uint8_t   comp : 7;
  bool      embedded : 1;

  uint8_t   cksum;
  uint8_t   type;
  uint8_t   lvl : 5;
  bool      encrypt : 1;
  bool      dedup : 1;
  bool      byte_order : 1;
} __attribute__((packed));

static_assert(sizeof(Props) == 8, "Props definition incorrect!");

struct Blkptr {
  Dva       vdev[3];
  Props     props;

  uint64_t  padding[3];
  uint64_t  birth_txg;
  uint64_t  fill;
  char      checksum[32];
} __attribute__((packed));

static_assert(sizeof(Blkptr) == 128, "Blkptr definition incorrect!");

struct Uberblock {
  uint64_t magic;
  uint64_t version;
  uint64_t txg;
  uint64_t guid_sum;
  uint64_t timestamp;
  Blkptr   blkptr;

  static std::unique_ptr<Uberblock> read(FILE *fp);
  bool readFrom(FILE *fp);
} __attribute__((packed));

#define SECTOR_TO_ADDR(x) ((x) << 9) // (((x) << 9) + 0x400000)