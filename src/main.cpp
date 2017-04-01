#include <memory>
#include <sys/stat.h> // mkdir
#include <vector>

#include "utils/array_view.h"
#include "utils/common.h"
#include "utils/file.h"
#include "utils/log.h"

#include "zfs/indirect_block.h"
#include "zfs/physical.h"
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

static bool extractFileContents(ZPoolReader &          reader,
                                const physical::DNode &dnode,
                                const std::string &    outFile) {
  ASSERT0(dnode.type == DNodeType::FileContents);

  LOG("Extracting file to %s...\n", outFile.c_str());

  IndirectBlock indirectBlock{reader, dnode};
  LOG("File total size: %zu, indirect block size: %zu, num data blocks: %zu\n",
      indirectBlock.size(), indirectBlock.indirectBlockSize(),
      indirectBlock.numDataBlocks());

  File fp{outFile, File::Write};
  if (!fp) {
    LOG("Failed to open output file!\n");
    return false;
  }

  for (BlockRef dataBlock : indirectBlock.blocks()) {
    LOG("Extracting block %p of size %zu...\n", dataBlock.data(),
        dataBlock.size());
    ASSERT0(std::fwrite(dataBlock.data(), dataBlock.size(), 1, fp) == 1);
  }

  LOG("Extraction complete!\n");
  return true;
}

enum class DirEntryFlags : u64 {
  Dir  = 0x4000000000000000,
  File = 0x8000000000000000
};

static std::size_t
extractDirContents(ZPoolReader &                      reader,
                   IndirectObjBlock<physical::DNode> &dslBlock,
                   const physical::DNode &dnode, const std::string &outDir) {
  ASSERT0(dnode.type == DNodeType::DirContents);

  LOG("Extracting directory to '%s'...\n", outDir.c_str());
  dnode.dump(stderr);

  const int mkresult = mkdir(outDir.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
  if (mkresult != 0 && mkresult != EEXIST) {
    LOG("Failed to create directory, error code: %d\n", mkresult);
    return 0;
  }

  auto dirZap = reader.read<MZapBlockPtr>(dnode.bps[0], /*dva=*/0);
  if (!dirZap) {
    LOG("Failed to read the ZAP block belonging to the DirContents DNode, "
        "skipping!\n");
    return 0;
  }

  std::size_t nfiles = 0;
  for (const physical::MZapEntry &entry : dirZap.entries()) {
    if (!entry.isValid())
      continue;

    if (flag_isset(entry.value, DirEntryFlags::Dir)) {
      const u64 nodeID = entry.value - static_cast<u64>(DirEntryFlags::Dir);
      nfiles +=
          extractDirContents(reader, dslBlock, dslBlock.objectByID(nodeID),
                             outDir + "/" + entry.name);
    } else if (flag_isset(entry.value, DirEntryFlags::File)) {
      const u64 nodeID = entry.value - static_cast<u64>(DirEntryFlags::File);
      if (extractFileContents(reader, dslBlock.objectByID(nodeID),
                              outDir + "/" + entry.name))
        nfiles++;
    } else {
      LOG("Unrecognised flag in directory ZAP entry, ignoring: ");
      entry.dump(stderr);
    }
  }

  return nfiles;
}

static bool handleMOS(ZPoolReader &                      reader,
                      IndirectObjBlock<physical::DNode> &mos) {
  physical::DNode *rootDatasetNode = getRootDataset(reader, mos);
  if (!rootDatasetNode) {
    LOG("Could not find the root dataset entry in an object directory!\n");
    return false;
  }

  auto rootDataset = rootDatasetNode->getBonusAs<physical::DSLDir>();
  // rootDataset->dump(stderr);

  const physical::DNode &headDatasetNode =
      mos.objectByID(rootDataset->head_dataset_obj);
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
  LOG("Extracting filesystem root (objid = %lu)...\n", rootDirObjID);

  const std::size_t nfiles = extractDirContents(
      reader, dslBlock, dslBlock.objectByID(rootDirObjID), "extracted2");
  LOG("Finished extracting %zu files!\n", nfiles);

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
