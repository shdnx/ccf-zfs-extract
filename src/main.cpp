#include <memory>
#include <vector>

#include "common.h"
#include "log.h"
#include "zfs-objects.h"
#include "zpool.h"

static void handle_dnode(ZPool &zpool, const DNode &dnode, unsigned level = 0) {
  ASSERT(level < 3, "Too many levels!!");

  if (!dnode.validate()) {
    LOG("Got invalid dnode, skipping!\n");
    return;
  }

  LOG(" -- Got a valid dnode:\n");
  dnode.dump(stderr);

  if (dnode.nlevels == 0) {
    LOG("!!!! It's a leaf dnode!\n");
  } else {
    LOG(" -- following dnode tree, levels = %u\n", dnode.nlevels);

    DNode child_dnode;
    for (size_t i = 0; i < dnode.nblkptr; i++) {
      for (size_t v = 0; v < 1; v++) {
        if (zpool.readObject(dnode.bps[i], /*vdev=*/v, OUT & child_dnode)) {
          handle_dnode(zpool, child_dnode, level + 1);
        }
      }
    }
  }
}

static void handle_ub(ZPool &zpool, const Uberblock &ub) {
  ub.dump(stderr);
  std::fprintf(stderr, "\n");

  ASSERT(ub.rootbp.type == DNodeType::ObjSet,
         "rootbp does not seem to point to an object!");

  ObjSet objset;
  for (size_t vdev_index = 0; vdev_index < 1; vdev_index++) {
    LOG(" -- following ub.rootbp with vdev = %zu\n", vdev_index);

    if (!zpool.readObject(ub.rootbp, vdev_index, OUT & objset)) {
      LOG("Uberblock rootbp: could not read root objset!\n");
      continue;
    }

    objset.dump(stderr);

    DNode dnode;
    for (size_t blkptr_index = 0; blkptr_index < objset.metadnode.nblkptr;
         blkptr_index++) {
      for (size_t vdev_index = 0; vdev_index < 1; vdev_index++) {
        LOG(" -- following root objset.metadnode.bps[%zu] with vdev = %zu\n",
            blkptr_index, vdev_index);

        const bool read = zpool.readObject(objset.metadnode.bps[blkptr_index],
                                           /*vdev=*/vdev_index, OUT & dnode);
        if (!read) {
          LOG("Warning: could not follow blkptr in root objset's metadnode!\n");
          continue;
        }

        handle_dnode(zpool, dnode);
      }
    }
  }
}

int main(int argc, const char **argv) {
  ASSERT(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *           path  = argv[1];
  std::unique_ptr<ZPool> zpool = ZPool::open(path);
  ASSERT(zpool, "Unable to open zpool file '%s'!\n", path);

  std::vector<Uberblock> ubs(VDEV_LABEL_NUBERBLOCKS);
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

  /*for (const Uberblock &ub : ubs) {
    if (ub.validate()) {
      handle_ub(*zpool, ub);
    }
  }*/

  return 0;
}

/*#define DBP_SPAN(dnp, level)          \
  (((uint64_t)dnp->dn_datablkszsec) << (SPA_MINBLOCKSHIFT + \
  (level) * (dnp->dn_indblkshift - SPA_BLKPTRSHIFT)))*/