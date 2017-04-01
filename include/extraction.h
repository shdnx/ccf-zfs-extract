#pragma once

#include <set>
#include <string>

#include "zfs/block.h"
#include "zfs/indirect_block.h"
#include "zfs/physical/dnode.h"
#include "zfs/zpool_reader.h"

bool extractFileContents(zfs::ZPoolReader &          reader,
                         const zfs::physical::DNode &dnode,
                         const std::string &         outFile);

std::size_t
extractDirContents(zfs::ZPoolReader &                           reader,
                   zfs::IndirectObjBlock<zfs::physical::DNode> &dslBlock,
                   const zfs::physical::DNode &dnode, const std::string &outDir,
                   std::set<const zfs::physical::DNode *> &extractedNodes);
