#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "byteorder.h"
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

enum class DumpFlags { None, AllowInvalid = 1 };

// unfortunately, we cannot use inheritance, because these data structures need
// to be directly readable from the disk for simplicity
#define VALID_IF(EXPR)                                                  \
  static constexpr const char *const validation_expr = #EXPR;           \
  bool                               isValid() const { return (EXPR); } \
  void dump(std::FILE *of, DumpFlags flags = DumpFlags::None) const;

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

  VALID_IF(asize != 0 && offset != 0);
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

// matches dmu_object_type in dmu.h in ZFS-on-Linux
enum class DNodeType : u8 {
  Invalid,
  ObjDirectory, // contains information about meta objects
  DNode  = 0x0a,
  ObjSet = 0x0b,
};

// See include/sys/zio.h for a full list of compressions that ZFS-on-Linux
// supports. The ZFS on-disk documentation here is not valid.
enum class Compress : u8 {
  Inherit = 0,
  On,
  Off,
  LZ4 = 0xf,

  Default = LZ4
};

struct Blkptr {
  Dva vdev[3];

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
  u64 flags;
  PADDING(432);
  DNode userused_dnode;
  DNode groupused_dnode;

  VALID_IF(true);
} __attribute__((packed));

static_assert(sizeof(ObjSet) == 2048, "ObjSet definition invalid!");

// ZAP = ZFS Attribute Processor
#define ZAP_MAGIC 0x2F52AB2ABull

// micro-zap, i.e. a ZAP that takes only one block, there's no indirection
// involved
struct MZapEntry {
  u64 value;
  u32 cd;
  PADDING(2);
  char name[50];

  VALID_IF(true);
} __attribute__((packed));

static_assert(sizeof(MZapEntry) == 64, "MZapEntry invalid!");

enum ZapBlockType : u64 {
  Leaf   = (1uLL << 63) + 0,
  Header = (1uLL << 63) + 1,
  Micro  = (1uLL << 63) + 3,
};

struct MZapHeader {
  ZapBlockType block_type;
  u64          salt;
  u64          normflags;
  PADDING(5 * sizeof(u64));
  MZapEntry entries[1]; // VLA

  static size_t getNumChunks(size_t block_size) {
    return 1 + (block_size - sizeof(MZapHeader)) / sizeof(MZapEntry);
  }

  // if it's not a Micro ZAP, then it cannot be represented as an MZap
  VALID_IF(block_type == ZapBlockType::Micro);
} __attribute__((packed));

struct MZapEntryIterator {
  using reference = const MZapEntry &;
  using pointer   = const MZapEntry *;

  explicit MZapEntryIterator(pointer entry) : m_entry{entry} {}

  reference operator*() const { return *m_entry; }
  pointer operator->() const { return m_entry; }

  MZapEntryIterator &operator++() {
    m_entry++;
    return *this;
  }

  MZapEntryIterator operator++(int) {
    MZapEntryIterator old{*this};
    m_entry++;
    return old;
  }

  bool operator==(const MZapEntryIterator &rhs) const {
    return m_entry == rhs.m_entry;
  }

  bool operator!=(const MZapEntryIterator &rhs) const {
    return !operator==(rhs);
  }

private:
  pointer m_entry;
};

struct MZapEntryRange {
  using iterator = MZapEntryIterator;

  explicit MZapEntryRange(const MZapEntry *begin, const MZapEntry *end)
      : m_begin{begin}, m_end{end} {}

  size_t size() const { return m_end - m_begin; }

  const MZapEntry &operator[](size_t i) const { return m_begin[i]; }

  iterator begin() const { return iterator{m_begin}; }
  iterator end() const { return iterator{m_end}; }

private:
  const MZapEntry *m_begin;
  const MZapEntry *m_end;
};

// NOTE: cannot be read directly from disk
struct MZapBlock {
  MZapEntryRange entries;
  MZapHeader     header; // variable length

  explicit MZapBlock(size_t block_size)
      : entries{&header.entries[0],
                &header.entries[MZapHeader::getNumChunks(block_size)]} {}

  explicit MZapBlock(MZapHeader header_, size_t block_size)
      : entries{&header.entries[0],
                &header.entries[MZapHeader::getNumChunks(block_size)]},
        header{header_} {}

  const MZapHeader *operator->() const { return &header; }

  size_t getNumEntries() const { return entries.size(); }

  const MZapEntry *findEntry(const std::string &name) const {
    for (const MZapEntry &entry : entries) {
      if (name == entry.name)
        return &entry;
    }

    return nullptr;
  }

  VALID_IF(header.isValid());
};
