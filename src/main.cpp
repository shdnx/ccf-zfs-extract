#include <cstdio>

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

  for (size_t vdev = 0; vdev < 1; vdev++) {
    std::fprintf(stdout, "\n ------ VDEV %zu ------ \n", vdev);

    if (!read_obj(fp, ub.rootbp.vdev[vdev].offset, OUT & metadnode)) {
      std::fprintf(stderr,
                   "Uberblock rootbp.vdev[%zu]: could not read metadnode!\n",
                   vdev);
      continue;
    }

    if (metadnode.nblkptr < 1 || 3 < metadnode.nblkptr) {
      std::fprintf(stderr,
                   "Ignored probably invalid metadnode, nblkptr = %02hhx\n",
                   metadnode.nblkptr);
      continue;
    }

    // fprintf(stdout, "metadnode raw dump:\n");
    // dump_raw(stdout, metadnode);

    Dump::dnode(stderr, metadnode);
  }
}

int main(int argc, const char **argv) {
  CRITICAL(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *path = argv[1];
  FILE *      fp   = std::fopen(path, "rb");
  CRITICAL(fp, "Unable to open zpool file %s!\n", path);

  // TODO: read all vdev labels for redundancy
  for (size_t i = 0; i < 128; i++) {
    std::fseek(fp, (128 + i) * 1024, SEEK_SET);

    auto ub = Uberblock::read(fp);
    if (ub) {
      handle_ub(fp, *ub);
    }
  }

  std::fclose(fp);
  return 0;
}
