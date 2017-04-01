#include <memory>
#include <vector>

#include "utils/array_view.h"
#include "utils/common.h"
#include "utils/log.h"
#include "zfs/indirect_block.h"
#include "zfs/physical/dnode.h"
#include "zfs/physical/mzap.h"
#include "zfs/physical/mzap.h"
#include "zfs/physical/objset.h"
#include "zfs/physical/uberblock.h"
#include "zfs/zpool_reader.h"

using namespace zfs;

static bool getRootDataset(ZPoolReader &reader, const physical::DNode &objDir,
                           OUT u64 *rootIndex) {
  for (std::size_t bp_index = 0; bp_index < objDir.nblkptr; bp_index++) {
    MZapBlockPtr zap;
    for (std::size_t dva = 0; dva < 3 && !zap; dva++) {
      zap = reader.read(objDir.bps[bp_index], dva).cast<MZapBlockPtr>();
    }

    if (!zap)
      continue;

    zap.dump(stderr);

    if (const physical::MZapEntry *entry = zap.findEntry("root_dataset")) {
      OUT *rootIndex = entry->value;
      return true;
    }
  }

  return false;
}

// TODO: the MOS should probably also be an indirect block
static physical::DNode *getRootDataset(ZPoolReader &                  reader,
                                       ArrayBlockRef<physical::DNode> mos) {
  for (const physical::DNode &dnode : mos.objects()) {
    if (!dnode.isValid())
      continue;

    if (dnode.type == DNodeType::ObjDirectory) {
      LOG("Found the object directory:\n");
      dnode.dump(stderr);

      u64 rootDatasetIndex;
      if (getRootDataset(reader, dnode, OUT & rootDatasetIndex))
        return &mos[rootDatasetIndex];
    }
  }

  return nullptr;
}

/*static bool dumpDirNode(FILE *fp, ZPoolReader &zpool, const DNode &dirNode) {
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

static bool handleMOS(ZPoolReader &reader, ArrayBlockRef<physical::DNode> mos) {
  /*for (size_t i = 0; i < mosLength; i++) {
    const DNode &dnode = mos[i];
    if (!dnode.isValid())
      continue;

    LOG("DNode of type %u\n", static_cast<u32>(dnode.type));
  }*/

  physical::DNode *rootDatasetNode = getRootDataset(reader, mos);
  if (!rootDatasetNode) {
    LOG("Could not find the root dataset entry in an object directory!\n");
    return false;
  }

  auto rootDataset = rootDatasetNode->getBonusAs<physical::DSLDir>();
  // rootDataset->dump(stderr);

  const physical::DNode &headDatasetNode = mos[rootDataset->head_dataset_obj];
  auto headDataset = headDatasetNode.getBonusAs<physical::DSLDataSet>();
  // headDataset->dump(stderr);

  auto dslObjSet =
      reader.read<ObjBlockPtr<physical::ObjSet>>(headDataset->bp, /*dva=*/0);
  dslObjSet->dump(stderr);

  // the master node always has the object id = 1
  IndirectObjBlock<physical::DNode> dslBlock{reader, dslObjSet->metadnode};
  const physical::DNode &           masterNode = dslBlock.objectByID(1);
  masterNode.dump(stderr);

  auto masterZap = reader.read<MZapBlockPtr>(masterNode.bps[0], /*dva=*/0);
  ASSERT0(masterZap && masterZap->isValid());

  masterZap.dump(stderr);

  const physical::MZapEntry *rootEntry = masterZap.findEntry("ROOT");
  if (!rootEntry) {
    LOG("Could not find the MZapEntry for the filesystem root!\n");
    return false;
  }

  const u64 rootDirObjID = rootEntry->value;
  LOG("filesystem ROOT entry obj id = %lu\n", rootDirObjID);

  for (std::size_t i = 0; i < dslBlock.numObjects(); i++) {
    const physical::DNode &dslNode = dslBlock.objectByID(i);
    if (!dslNode.isValid())
      continue;

    LOG("DSLNodes[%zu]: ", i);
    dslNode.dump(stderr);
  }

#if 0
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
#endif

  return true;
}

static void handle_ub(ZPoolReader &zpool, const physical::Uberblock &ub) {
  ub.dump(stderr);
  std::fprintf(stderr, "\n");

  ASSERT(ub.rootbp.type == DNodeType::ObjSet,
         "rootbp does not seem to point to an object!");

  ObjBlockPtr<physical::ObjSet> objset;
  for (size_t root_dva = 0; root_dva < 3; root_dva++) {
    LOG(" -- following ub.rootbp with dva = %zu\n", root_dva);
    if (!zpool.read(ub.rootbp, root_dva, OUT & objset)) {
      LOG("Uberblock rootbp: could not read root objset!\n");
      return;
    }

    objset->dump(stderr);

    ArrayBlockPtr<physical::DNode> dnodes;
    for (size_t blkptr_index = 0; blkptr_index < objset->metadnode.nblkptr;
         blkptr_index++) {
      const physical::Blkptr &bp = objset->metadnode.bps[blkptr_index];
      if (!bp.isValid())
        continue;

      for (size_t dva_index = 0; dva_index < 3; dva_index++) {
        if (!bp.dva[dva_index].isValid())
          continue;

        LOG(" -- following root objset.metadnode.bps[%zu] with dva = %zu\n",
            blkptr_index, dva_index);

        if (!zpool.read(bp, dva_index, OUT & dnodes)) {
          LOG("Failed to read MOS object array!\n");
          continue;
        }

        if (handleMOS(zpool, dnodes))
          return;
      }
    }
  }
}

int main(int argc, const char **argv) {
  ASSERT(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *                 path  = argv[1];
  std::unique_ptr<ZPoolReader> zpool = ZPoolReader::open(path);
  ASSERT(zpool, "Unable to open zpool file '%s'!\n", path);

  std::vector<physical::Uberblock> ubs(VDEV_LABEL_NUBERBLOCKS);
  ssize_t                          max_txg_index = -1;

  for (u32 i = 0; i < VDEV_LABEL_NUBERBLOCKS; i++) {
    physical::Uberblock &ub = ubs[i];

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
