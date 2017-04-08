#include <sys/stat.h> // mkdir

#include "zfs/indirect_block.h"
#include "zfs/physical/mzap.h"
#include "zfs/physical/znode.h"

#include "utils/common.h"
#include "utils/file.h"
#include "utils/log.h"

#include "extraction.h"

using namespace zfs;

enum class DirEntryFlags : u64 {
  Dir  = 0x4000000000000000,
  File = 0x8000000000000000
};

bool extractFileContents(ZPoolReader &reader, const physical::DNode &dnode,
                         const std::string &outFile) {
  ASSERT0(dnode.type == DNodeType::FileContents);

  LOG("Extracting file to %s...\n", outFile.c_str());

  IndirectBlock indirectBlock{reader, dnode};
  LOG("File blocks total size: %zu, indirect block size: %zu, num data blocks: "
      "%zu\n",
      indirectBlock.size(), indirectBlock.indirectBlockSize(),
      indirectBlock.numDataBlocks());

  File fp{outFile, File::Write};
  if (!fp) {
    LOG("Failed to open output file!\n");
    return false;
  }

  const auto &znode = dnode.getBonusAs<physical::ZNode>();
  znode.dump(stderr);

  LOG("Actual file size: %lu\n", znode.size);

  const std::size_t fileSize = znode.size;

  std::size_t writtenSize = 0;
  for (BlockRef dataBlock : indirectBlock.blocks()) {
    const std::size_t writeSize =
        std::min(dataBlock.size(), fileSize - writtenSize);
    // const std::size_t writeSize = dataBlock.size();

    LOG("Extracting block %p of size %zu, writing effective length %zu...\n",
        dataBlock.data(), dataBlock.size(), writeSize);
    ASSERT0(std::fwrite(dataBlock.data(), writeSize, 1, fp) == 1);

    writtenSize += writeSize;
  }

  LOG("Extraction complete!\n");
  return true;
}

std::size_t
extractDirContents(ZPoolReader &                      reader,
                   IndirectObjBlock<physical::DNode> &dslBlock,
                   const physical::DNode &dnode, const std::string &outDir,
                   std::set<const physical::DNode *> &extractedNodes) {
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

    std::string entryPath = outDir + "/" + entry.name;

    if (flag_isset(entry.value, DirEntryFlags::Dir)) {
      const u64 nodeID = entry.value - static_cast<u64>(DirEntryFlags::Dir);
      const physical::DNode &dirNode = dslBlock.objectByID(nodeID);

      try {
        nfiles += extractDirContents(reader, dslBlock, dirNode, entryPath,
                                     extractedNodes);

        extractedNodes.insert(&dirNode);
      } catch (const std::exception &ex) {
        LOG("Error: cannot extract directory contents of %s: %s\n",
            entryPath.c_str(), ex.what());
      }
    } else if (flag_isset(entry.value, DirEntryFlags::File)) {
      const u64 nodeID = entry.value - static_cast<u64>(DirEntryFlags::File);
      const physical::DNode &fileNode = dslBlock.objectByID(nodeID);

      if (extractFileContents(reader, fileNode, entryPath)) {
        extractedNodes.insert(&fileNode);
        nfiles++;
      }
    } else {
      LOG("Unrecognised flag in directory ZAP entry, ignoring: ");
      entry.dump(stderr);
    }
  }

  extractedNodes.insert(&dnode);
  return nfiles;
}
