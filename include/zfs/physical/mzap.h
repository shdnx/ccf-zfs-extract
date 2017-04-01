#pragma once

#include <string>

#include "zfs/block.h"
#include "zfs/general.h"

// ZAP = ZFS Attribute Processor

namespace zfs {
namespace physical {

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

  static std::size_t getNumChunks(std::size_t block_size) {
    return (block_size - sizeof(MZapHeader)) / sizeof(MZapEntry);
  }

  // if it's not a Micro ZAP, then it cannot be represented as an MZap
  VALID_IF(block_type == ZapBlockType::Micro);
  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
} __attribute__((packed));

} // end namespace physical

struct MZapBlockPtr
    : HeadedBlockPtr<physical::MZapHeader, physical::MZapEntry> {
  GEN_BLOCKPTR_SUPPORT(
      MZapBlockPtr,
      REFL(HeadedBlockPtr<physical::MZapHeader, physical::MZapEntry>)) {
    ASSERT0(header()->block_type == physical::ZapBlockType::Micro);
  }

  const physical::MZapEntry *findEntry(const std::string &name) {
    for (const physical::MZapEntry &entry : entries()) {
      if (name == entry.name)
        return &entry;
    }

    return nullptr;
  }

  void dump(std::FILE *fp, DumpFlags flags = DumpFlags::None) const;
};

} // end namespace zfs
