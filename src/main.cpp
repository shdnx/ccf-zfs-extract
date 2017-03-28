#include <memory>
#include <vector>

#include "array_view.h"
#include "common.h"
#include "log.h"
#include "zfs-objects.h"
#include "zpool.h"

/*static void handle_dnode(ZPool &zpool, const DNode &dnode, unsigned level = 0)
{
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
        if (zpool.readObject(dnode.bps[i], v, OUT & child_dnode)) {
          handle_dnode(zpool, child_dnode, level + 1);
        }
      }
    }
  }
}*/

static bool getRootDataset(ZPool &zpool, const DNode &objDir,
                           OUT u64 *root_index) {
  for (size_t bp_index = 0; bp_index < objDir.nblkptr; bp_index++) {
    const Blkptr &bp = objDir.bps[bp_index];
    MZapBlock     zap{bp.getLogicalSize()};

    if (!zpool.readVLObject(bp, 0, OUT zap.data()))
      continue;

    ASSERT(zap->block_type == ZapBlockType::Micro,
           "Cannot handle non-micro ZAP!");

    zap.dump(stderr);

    if (const MZapEntry *entry = zap.findEntry("root_dataset")) {
      OUT *root_index = entry->value;
      return true;
    }
  }

  return false;
}

static bool getRootDataset(ZPool &zpool, const DNode *mos, size_t mosLength,
                           OUT DNode const **rootDataset) {
  for (size_t i = 0; i < mosLength; i++) {
    const DNode &dnode = mos[i];
    if (!dnode.isValid())
      continue;

    if (dnode.type == DNodeType::ObjDirectory) {
      LOG("Found the object directory:\n");
      dnode.dump(stderr);

      u64 rootDatasetIndex;
      if (getRootDataset(zpool, dnode, OUT & rootDatasetIndex)) {
        OUT *rootDataset = &mos[rootDatasetIndex];
        return true;
      }
    }
  }

  return false;
}

static const DNode *getDSLNodes(ZPool &zpool, const ObjSet &dslObjSet,
                                OUT std::unique_ptr<DNode[]> *ll_nodes,
                                OUT size_t *nll_nodes) {
  // the master node always has the object id = 1, so we'll just take the first
  // blkptr on all levels

  Blkptr                    bp = dslObjSet.metadnode.bps[0];
  std::unique_ptr<Blkptr[]> bp_array;
  size_t                    bp_array_len;

  for (size_t lvl = dslObjSet.metadnode.nlevels; lvl > 1; lvl--) {
    // we read all blkptrs, so that it's easy to generalize this function later
    bp_array_len = zpool.readObjectArray(bp, /*dva_index=*/0, OUT & bp_array);
    ASSERT(bp_array_len != 0, "Failed to read blkptr object array!");

    bp = bp_array[0]; // copy
  }

  std::unique_ptr<DNode[]> dnodes;
  size_t ndnodes = zpool.readObjectArray(bp, /*dva_index=*/0, OUT & dnodes);
  ASSERT0(ndnodes != 0);

  const DNode *masterNode = &dnodes[1];

  OUT *nll_nodes = ndnodes;
  OUT *ll_nodes  = std::move(dnodes);
  return masterNode;
}

/*static bool dumpDirNode(FILE *fp, ZPool &zpool, const DNode &dirNode) {
  const Blkptr &bp = dirNode.bps[0];
  MZapBlock     zap{bp.getLogicalSize()};

  if (!zpool.readVLObject(bp, /*dva_index=* /0, OUT zap.data())) {
    return false;
  }

  ASSERT(zap->block_type == ZapBlockType::Micro,
         "Cannot handle non-micro ZAP!");

  for (const MZapEntry &entry : zap.entries()) {
    std::fprintf(stderr, "")
  }

  return true;
}*/

static bool handleMOS(ZPool &zpool, const DNode *mos, size_t mosLength) {
  /*for (size_t i = 0; i < mosLength; i++) {
    const DNode &dnode = mos[i];
    if (!dnode.isValid())
      continue;

    LOG("DNode of type %u\n", static_cast<u32>(dnode.type));
  }*/

  const DNode *rootDatasetNode;
  if (!getRootDataset(zpool, mos, mosLength, OUT & rootDatasetNode)) {
    LOG("Could not find the root dataset entry in an object directory!\n");
    return false;
  }

  const DSLDir *rootDataset = rootDatasetNode->getBonusAs<DSLDir>();
  // rootDataset->dump(stderr);

  const DNode &     headDatasetNode = mos[rootDataset->head_dataset_obj];
  const DSLDataSet *headDataset     = headDatasetNode.getBonusAs<DSLDataSet>();
  // headDataset->dump(stderr);

  ObjSet rootObjSet;
  for (size_t root_dva = 0; root_dva < 2; root_dva++) {
    if (!zpool.readObject(headDataset->bp, root_dva, OUT & rootObjSet)) {
      LOG("Failed to read root ObjSet from dva %zu!\n", root_dva);
      continue;
    }

    break;
  }

  rootObjSet.dump(stderr);

  std::unique_ptr<DNode[]> dslNodes;
  size_t                   ndslnodes;
  const DNode *            masterNode =
      getDSLNodes(zpool, rootObjSet, OUT & dslNodes, OUT & ndslnodes);
  if (!masterNode) {
    LOG("Could not get master node!\n");
    return false;
  }

  masterNode->dump(stderr);

  const Blkptr &masterBP = masterNode->bps[0];
  MZapBlock     masterZAP{masterBP.getLogicalSize()};

  if (!zpool.readVLObject(masterBP, /*dva=*/0, OUT masterZAP.data())) {
    LOG("Could not read the master ZAP block!\n");
    return false;
  }

  ASSERT0(masterZAP.isValid());

  masterZAP.dump(stderr);

  const MZapEntry *rootEntry = masterZAP.findEntry("ROOT");
  if (!rootEntry) {
    LOG("Could not find the MZapEntry for the filesystem root!\n");
    return false;
  }

  const u64 rootDirIndex = rootEntry->value;
  LOG("filesystem ROOT entry = %lu\n", rootDirIndex);

  for (size_t i = 0; i < ndslnodes; i++) {
    LOG("DSLNodes[%zu]: ", i);
    dslNodes[i].dump(stderr);
  }

  const DNode &rootDirNode = dslNodes[rootDirIndex];
  rootDirNode.dump(stderr, DumpFlags::AllowInvalid);

  MZapBlock rootDirZAP{rootDirNode.bps[0].getLogicalSize()};
  if (!zpool.readVLObject(rootDirNode.bps[0], /*dva=*/0,
                          OUT rootDirZAP.data())) {
    LOG("Could not read ZAP block for root directory!\n");
    return false;
  }

  // ASSERT0(rootDirZAP.isValid());
  rootDirZAP.dump(stderr);

  return true;
}

static void handle_ub(ZPool &zpool, const Uberblock &ub) {
  ub.dump(stderr);
  std::fprintf(stderr, "\n");

  ASSERT(ub.rootbp.type == DNodeType::ObjSet,
         "rootbp does not seem to point to an object!");

  ObjSet objset;
  for (size_t root_dva = 0; root_dva < 3; root_dva++) {
    LOG(" -- following ub.rootbp with dva = %zu\n", root_dva);
    if (!zpool.readObject(ub.rootbp, root_dva, OUT & objset)) {
      LOG("Uberblock rootbp: could not read root objset!\n");
      return;
    }

    objset.dump(stderr);

    std::unique_ptr<DNode[]> dnodes;
    for (size_t blkptr_index = 0; blkptr_index < objset.metadnode.nblkptr;
         blkptr_index++) {
      const Blkptr &bp = objset.metadnode.bps[blkptr_index];
      if (!bp.isValid())
        continue;

      for (size_t dva_index = 0; dva_index < 3; dva_index++) {
        if (!bp.dva[dva_index].isValid())
          continue;

        LOG(" -- following root objset.metadnode.bps[%zu] with dva = %zu\n",
            blkptr_index, dva_index);

        const size_t nread = zpool.readObjectArray(bp, dva_index, OUT & dnodes);
        if (!nread) {
          LOG("Failed to read MOS object array!\n");
          continue;
        }

        if (handleMOS(zpool, dnodes.get(), nread))
          return;
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
  return 0;
}
