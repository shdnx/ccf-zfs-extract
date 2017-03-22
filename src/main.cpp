#include <memory>
#include <vector>

#include "common.h"
#include "log.h"
#include "zfs-objects.h"
#include "zpool.h"

static void handle_ub(ZPool &zpool, const Uberblock &ub) {
  ub.dump(stderr);
  std::fprintf(stderr, "\n");

  ASSERT(ub.rootbp.type == BlkptrType::ObjSet,
         "rootbp does not seem to point to an object!");

  ObjSet objset;
  for (size_t vdev_index = 0; vdev_index < 1; vdev_index++) {
    LOG("vdev = %zu\n", vdev_index);

    ASSERT(zpool.readObject(ub.rootbp, vdev_index, OUT & objset),
           "Uberblock rootbp: could not read root objset!");

    objset.dump(stderr);

    DNode dnode;
    for (size_t blkptr_index = 0; blkptr_index < objset.metadnode.nblkptr;
         blkptr_index++) {
      ASSERT(
          zpool.readObject(objset.metadnode.bps[blkptr_index], 0, OUT & dnode),
          "Could not follow blkptr in root objset's metadnode!");

      dnode.dump(stderr);
    }
  }
}

int main(int argc, const char **argv) {
  ASSERT(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *           path  = argv[1];
  std::unique_ptr<ZPool> zpool = ZPool::open(path);
  ASSERT(zpool, "Unable to open zpool file '%s'!\n", path);

  std::vector<Uberblock> ubs(128);
  ssize_t                max_txg_index = -1;

  for (u32 i = 0; i < VDEV_LABEL_NUBERBLOCKS; i++) {
    Uberblock &ub = ubs[i];

    if (zpool->readUberblock(/*label_index=*/0, i, OUT & ub)) {
      if (max_txg_index == -1 || ubs[max_txg_index].txg < ub.txg)
        max_txg_index = i;
    }
  }

  if (max_txg_index == -1) {
    std::fprintf(stderr, "No valid uberblocks found!\n");
    return 1;
  }

  handle_ub(*zpool, ubs[max_txg_index]);
  return 0;
}
