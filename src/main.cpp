#include <cassert>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

#include "lz4.h"

#include "common.h"
#include "zfs-dump.h"
#include "zfs.h"

template <typename T> static bool read_obj(FILE *fp, size_t secno, OUT T *obj) {
  size_t addr = SECTOR_TO_ADDR(secno);
  fprintf(stderr, "reading object from addr: %016lx\n", addr);
  std::fseek(fp, addr, SEEK_SET);

  size_t nread = std::fread(obj, sizeof(T), 1, fp);
  if (nread != 1)
    return false;

  return true;
}

// stolen from byteorder.h in linux-on-zfs source tree
#define BE_IN8(xa) *((uint8_t *)(xa))

#define BE_IN16(xa) (((uint16_t)BE_IN8(xa) << 8) | BE_IN8((uint8_t *)(xa) + 1))

#define BE_IN32(xa) \
  (((uint32_t)BE_IN16(xa) << 16) | BE_IN16((uint8_t *)(xa) + 2))

#define BE_IN64(xa) \
  (((uint64_t)BE_IN32(xa) << 32) | BE_IN32((uint8_t *)(xa) + 4))

template <typename T>
static bool read_compressed_obj(FILE *fp, size_t addr, size_t phys_size,
                                size_t logical_size, OUT T *obj) {
  assert(phys_size % SECTOR_SIZE == 0);

  std::unique_ptr<char[]> ibuffer{new char[phys_size]};

  std::fprintf(stderr, "reading compressed object from addr: %016lx\n", addr);
  if (std::fseek(fp, addr, SEEK_SET) != 0) {
    std::fprintf(stderr, "seek failed!\n");
    return false;
  }

  const size_t nread = std::fread(ibuffer.get(), sizeof(char), phys_size, fp);
  if (nread != phys_size) {
    std::fprintf(stderr, "failed to read compressed object of psize = %lx, "
                         "could only read: %zu\n",
                 phys_size, nread);
    return false;
  }

  std::unique_ptr<char[]> obuffer{new char[logical_size]};

  u32 *     ibuffer_raw     = reinterpret_cast<u32 *>(ibuffer.get());
  const u32 compressed_size = BE_IN32(ibuffer_raw);
  assert(logical_size > compressed_size + sizeof(compressed_size));

  const int nbytes = LZ4_decompress_safe(
      reinterpret_cast<const char *>(&ibuffer_raw[1]), obuffer.get(),
      static_cast<int>(compressed_size), static_cast<int>(logical_size));

  std::memcpy(obj, obuffer.get(), sizeof(T));
  return true;
}

template <typename TObj>
static bool read_compressed_obj(FILE *fp, const Blkptr &bp, OUT TObj *obj) {
  return read_compressed_obj(fp, bp.vdev[0].getAddress(), bp.getPhysicalSize(),
                             bp.getLogicalSize(), OUT obj);
}

static void dump_raw(FILE *fp, const void *ptr, size_t sz) {
  assert(sz % 8 == 0);

  for (size_t i = 0, nqwords = sz / 8; i < nqwords; i++) {
    fprintf(fp, "%016lx\n", static_cast<const u64 *>(ptr)[i]);
  }
}

template <typename T> static void dump_raw(FILE *fp, const T &obj) {
  dump_raw(fp, static_cast<const void *>(&obj), sizeof(T));
}

static void handle_ub(FILE *fp, const Uberblock &ub) {
  const size_t ub_offset = static_cast<size_t>(ftell(fp) - sizeof(Uberblock));

  std::fprintf(stdout, "Found Uberblock @ %08lx\n", ub_offset);
  Dump::uberblock(stderr, ub);
  std::fprintf(stderr, "\n");

  if (ub.rootbp.props.type != BlkptrType::ObjSet) {
    std::fprintf(stderr,
                 "rootbp does not seem to point to an objset, ignoring!\n");
    return;
  }

  if (ub.rootbp.props.endian != Endianness::Little) {
    std::fprintf(stderr, "rootbp not little endian, ignoring!\n");
    return;
  }

  DNode metadnode;
  CRITICAL(read_compressed_obj(fp, ub.rootbp, OUT & metadnode),
           "Uberblock rootbp: could not read metadnode!\n");

  assert(1 <= metadnode.nblkptr && metadnode.nblkptr <= 3);
  Dump::dnode(stderr, metadnode);
}

int main(int argc, const char **argv) {
  CRITICAL(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *path = argv[1];
  FILE *      fp   = std::fopen(path, "rb");
  CRITICAL(fp, "Unable to open zpool file %s!\n", path);

  std::vector<std::unique_ptr<Uberblock>> ubs(128);
  ssize_t                                 max_txg_index = -1;

  // TODO: read all vdev labels for redundancy
  for (size_t i = 0; i < 128; i++) {
    std::fseek(fp, (128 + i) * 1024, SEEK_SET);

    auto ub = Uberblock::read(fp);
    if (ub) {
      if (max_txg_index == -1 || ubs[max_txg_index]->txg < ub->txg)
        max_txg_index = ubs.size();

      ubs.push_back(std::move(ub));
    }
  }

  handle_ub(fp, *ubs[max_txg_index]);

  std::fclose(fp);
  return 0;
}
