#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "byteorder.h"
#include "common.h"
#include "ptr_range.h"

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
#define VALID_IF(EXPR)                                        \
  static constexpr const char *const validation_expr = #EXPR; \
  bool                               isValid() const { return (EXPR); }

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
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

static_assert(sizeof(Dva) == 16, "Dva definition incorrect!");

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
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
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
  void dump(FILE *fp, DumpFlags flags = DumpFlags::None) const;
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
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
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
  void dump(FILE *fp, DumpFlags flags = DumpFlags::None) const;
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

  VALID_IF(name[0] != 0);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
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
  MZapEntry entries[]; // VLA

  static size_t getNumChunks(size_t block_size) {
    return (block_size - sizeof(MZapHeader)) / sizeof(MZapEntry);
  }

  // if it's not a Micro ZAP, then it cannot be represented as an MZap
  VALID_IF(block_type == ZapBlockType::Micro);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

template <typename THeader, typename TEntry>
struct ZFSBlock {
  using entry_range = ConstPtrRange<TEntry>;

  explicit ZFSBlock(size_t size)
      : m_size{size}, m_data{new char[size]},
        m_entries{
            reinterpret_cast<const TEntry *>(m_data.get() + sizeof(THeader)),
            reinterpret_cast<const TEntry *>(m_data.get() + size)} {
    ASSERT(size >= sizeof(THeader),
           "Block header size (%zu) is less than total block size (%zu)!",
           sizeof(THeader), size);
    ASSERT((size - sizeof(THeader)) % sizeof(TEntry) == 0,
           "Misaligned header or entries: block size = %zu, header size = %zu, "
           "entry size = %zu",
           size, sizeof(THeader), sizeof(TEntry));
  }

  ZFSBlock(const ZFSBlock &) = delete;

  ZFSBlock(ZFSBlock &&rhs)
      : m_size{rhs.m_size}, m_data{std::move(rhs.m_data)},
        m_entries{std::move(rhs.m_entries)} {}

  ZFSBlock &operator=(const ZFSBlock &rhs) = delete;

  ZFSBlock &operator=(ZFSBlock &&rhs) {
    m_size    = rhs.m_size;
    m_data    = std::move(rhs.m_data);
    m_entries = std::move(rhs.m_entries);
    return *this;
  }

  void *      data() { return m_data.get(); }
  const void *data() const { return m_data.get(); }

  size_t size() const { return m_size; }

  THeader *header() { return reinterpret_cast<THeader *>(data()); }

  const THeader *header() const {
    return reinterpret_cast<const THeader *>(data());
  }

  const THeader *operator->() const { return header(); }

  const entry_range &entries() const { return m_entries; }
  size_t             numEntries() const { return entries().size(); }

  VALID_IF(header() && header()->isValid());

private:
  size_t                  m_size;
  std::unique_ptr<char[]> m_data;
  entry_range             m_entries;
};

struct MZapBlock : ZFSBlock<MZapHeader, MZapEntry> {
  explicit MZapBlock(size_t block_size) : ZFSBlock{block_size} {}

  const MZapEntry *findEntry(const std::string &name) const {
    for (const MZapEntry &entry : entries()) {
      if (name == entry.name)
        return &entry;
    }

    return nullptr;
  }

  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
};

// this lives in the bonus area of DNode objects
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
