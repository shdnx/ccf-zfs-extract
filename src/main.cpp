#include <cstring>
#include <memory>
#include <set>
#include <vector>

#include "utils/array_view.h"
#include "utils/common.h"
#include "utils/file.h"
#include "utils/log.h"

#include "zfs/indirect_block.h"
#include "zfs/physical.h"
#include "zfs/zpool_reader.h"

#include "extraction.h"

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

static physical::DNode *getRootDataset(ZPoolReader &reader,
                                       IndirectObjBlock<physical::DNode> &mos) {
  for (const physical::DNode &dnode : mos.objects()) {
    if (!dnode.isValid())
      continue;

    if (dnode.type == DNodeType::ObjDirectory) {
      LOG("Found the object directory:\n");
      dnode.dump(stderr);

      u64 rootDatasetIndex;
      if (getRootDataset(reader, dnode, OUT & rootDatasetIndex))
        return &mos.objectByID(rootDatasetIndex);
    }
  }

  return nullptr;
}

static bool handleMOS(ZPoolReader &                      reader,
                      IndirectObjBlock<physical::DNode> &mos) {
  physical::DNode *rootDatasetNode = getRootDataset(reader, mos);
  if (!rootDatasetNode) {
    LOG("Could not find the root dataset entry in an object directory!\n");
    return false;
  }

  auto &rootDataset = rootDatasetNode->getBonusAs<physical::DSLDir>();

  const physical::DNode &headDatasetNode =
      mos.objectByID(rootDataset.head_dataset_obj);
  auto &headDataset = headDatasetNode.getBonusAs<physical::DSLDataSet>();

  auto dslObjSet =
      reader.read<ObjBlockPtr<physical::ObjSet>>(headDataset.bp, /*dva=*/0);
  dslObjSet->dump(stderr);

  // the master node always has the object id = 1
  IndirectObjBlock<physical::DNode> dslBlock{reader, dslObjSet->metadnode};

  /*size_t i = 0;
  for (const physical::DNode &dn : dslBlock.objects()) {
    fprintf(stderr, "Object[%zu]: ", i);
    dn.dump(stderr, DumpFlags::AllowInvalid);

    i++;
  }

  return true;*/

  const physical::DNode &masterNode = dslBlock.objectByID(1);
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
  LOG("Extracting filesystem root (objid = %lu)...\n", rootDirObjID);

  std::set<const physical::DNode *> extractedNodes;
  std::size_t                       nfiles = 0;

  try {
    nfiles =
        extractDirContents(reader, dslBlock, dslBlock.objectByID(rootDirObjID),
                           "extracted", extractedNodes);
    LOG("Finished extracting %zu files!\n", nfiles);
  } catch (const std::exception &ex) {
    LOG("Could not extract the root directory: %s\n", ex.what());
  }

  LOG("Looking for an extracting unreferenced files and directories...\n");
  int counter = -1;
  for (const physical::DNode &dnode : dslBlock.objects()) {
    counter++;
    if (!dnode.isValid() || extractedNodes.count(&dnode))
      continue;

    if (dnode.type == DNodeType::DirContents) {
      try {
        extractDirContents(reader, dslBlock, dnode,
                           "extracted_dangling_dir" + std::to_string(counter),
                           extractedNodes);
      } catch (const std::exception &ex) {
        LOG("Failed to extract dangling directory (node ID) %d: %s\n", counter,
            ex.what());
      }
    } else if (dnode.type == DNodeType::FileContents) {
      try {
        extractFileContents(reader, dnode, "extracted_dangling_file" +
                                               std::to_string(counter));
        extractedNodes.insert(&dnode);
      } catch (const std::exception &ex) {
        LOG("Failed to extract dangling file (node ID %d): %s\n", counter,
            ex.what());
      }
    }
  }

  LOG("All done!\n");
  return true;
}

static void handle_ub(ZPoolReader &reader, const physical::Uberblock &ub) {
  ub.dump(stderr);
  std::fprintf(stderr, "\n");

  ASSERT(ub.rootbp.type == DNodeType::ObjSet,
         "rootbp does not seem to point to an object!");

  ObjBlockPtr<physical::ObjSet> objset;
  if (!reader.read(ub.rootbp, /*dva=*/0, OUT & objset)) {
    LOG("Uberblock rootbp: could not read root objset!\n");
    return;
  }

  objset->dump(stderr);

  IndirectObjBlock<physical::DNode> objsetBlock{reader, objset->metadnode};
  handleMOS(reader, objsetBlock);
}

static void list_ubs(ZPoolReader &                           reader,
                     const std::vector<physical::Uberblock> &ubs,
                     ssize_t                                 maxTxgIndex) {
  std::size_t i = 0;
  for (const physical::Uberblock &ub : ubs) {
    if (i == maxTxgIndex) {
      std::fprintf(stderr, "[ACTIVE] ");
    }

    std::fprintf(stderr, "Uberblock[%zu]: ", i);
    ub.dump(stderr);
    i++;
  }

  std::fprintf(stderr, "Active Uberblock at index %zu\n", maxTxgIndex);
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

  if (argc >= 3 && std::strcmp(argv[2], "--list-uberblocks") == 0) {
    list_ubs(*zpool, ubs, max_txg_index);
    return 0;
  } else if (argc >= 3 && std::strcmp(argv[2], "--extract") == 0) {
    long ubIndex = max_txg_index;
    if (argc >= 4) {
      ubIndex = std::atol(argv[3]);

      if (ubIndex >= VDEV_LABEL_NUBERBLOCKS) {
        std::fprintf(stderr, "Invalid uberblock index!\n");
        return 1;
      }
    }

    handle_ub(*zpool, ubs[ubIndex]);
  } else {
    std::fprintf(stderr, "Please specify either --list-uberblocks or --extract "
                         "<uberblock index>\n");
  }

  return 0;
}
