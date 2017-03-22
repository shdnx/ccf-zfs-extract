#include <cstring>

#include "lz4.h"

#include "log.h"
#include "zpool.h"

#define VDEV_LABEL_SIZE (KB * 256)

static bool seekToLabel(std::FILE *fp, u32 label_index) {
  switch (label_index) {
  case 0:
    return std::fseek(fp, 0, SEEK_SET) == 0;

  case 1:
    return std::fseek(fp, static_cast<long>(VDEV_LABEL_SIZE), SEEK_SET) == 0;

  case 3:
    return std::fseek(fp, -static_cast<long>(VDEV_LABEL_SIZE), SEEK_END) == 0;

  case 4:
    return std::fseek(fp, -static_cast<long>(VDEV_LABEL_SIZE * 2), SEEK_END) ==
           0;

  default:
    UNREACHABLE("Invalid label index: %u!", label_index);
  }
}

bool ZPool::readUberblock(u32 label_index, u32 ub_index, OUT Uberblock *ub) {
  if (!seekToLabel(m_fp, label_index)) {
    LOG("Could not seek to label L%u!\n", label_index);
    return false;
  }

  long offset = static_cast<long>(KB * (128 + ub_index));
  std::fseek(m_fp, offset, SEEK_CUR);

  size_t nread = std::fread(ub, sizeof(Uberblock), 1, m_fp);
  if (nread != 1) {
    LOG("Uberblock L%u:%u could not be read from file!\n", label_index,
        ub_index);
    return false;
  }

  if (!ub->validate()) {
    // LOG("Uberblock L%u:%u failed validation check!\n", label_index,
    // ub_index);
    return false;
  }

  return true;
}

// blatantly stolen from byteorder.h in linux-on-zfs source tree
#define BE_IN8(xa) *((uint8_t *)(xa))

#define BE_IN16(xa) (((uint16_t)BE_IN8(xa) << 8) | BE_IN8((uint8_t *)(xa) + 1))

#define BE_IN32(xa) \
  (((uint32_t)BE_IN16(xa) << 16) | BE_IN16((uint8_t *)(xa) + 2))

#define BE_IN64(xa) \
  (((uint64_t)BE_IN32(xa) << 32) | BE_IN32((uint8_t *)(xa) + 4))

static bool readLZ4CompressedObject(FILE *fp, size_t logical_size,
                                    size_t phys_size, size_t obj_size,
                                    OUT void *obj) {
  ASSERT(phys_size % SECTOR_SIZE == 0, "Non-sector aligned physical size: %zu",
         phys_size);

  std::unique_ptr<char[]> ibuffer{new char[phys_size]};

  const size_t nread = std::fread(ibuffer.get(), sizeof(char), phys_size, fp);
  if (nread != phys_size) {
    LOG("Failed to read compressed object of psize = %lx, "
        "could only read: %zu\n",
        phys_size, nread);
    return false;
  }

  std::unique_ptr<char[]> obuffer{new char[logical_size]};

  u32 *     ibuffer_raw     = reinterpret_cast<u32 *>(ibuffer.get());
  const u32 compressed_size = BE_IN32(ibuffer_raw);
  ASSERT(logical_size > compressed_size + sizeof(compressed_size),
         "Invalid logical size %zu: lower than the compressed size %zu",
         logical_size, compressed_size);

  const int nbytes = LZ4_decompress_safe(
      reinterpret_cast<const char *>(&ibuffer_raw[1]), obuffer.get(),
      static_cast<int>(compressed_size), static_cast<int>(logical_size));

  LOG("LZ4_decompress_safe => %d\n", nbytes);

  std::memcpy(obj, obuffer.get(), obj_size);
  return true;
}

bool ZPool::_readObjectImpl(const Blkptr &bp, u32 vdev_index, size_t obj_size,
                            OUT void *obj) {
  ASSERT(bp.endian == Endian::Little, "Cannot handle big endian blkptr!");

  const u64 addr = bp.vdev[vdev_index].getAddress();

  LOG("Reading object of size %zu from offset: %016lx\n", obj_size, addr);
  if (std::fseek(m_fp, addr, SEEK_SET) != 0) {
    LOG("Seek failed!\n");
    return false;
  }

  const size_t lsize = bp.getLogicalSize();
  const size_t psize = bp.getPhysicalSize();

  const Compress effectiveComp =
      (bp.comp == Compress::On ? Compress::Default : bp.comp);

  switch (effectiveComp) {
  case Compress::LZ4:
    return readLZ4CompressedObject(m_fp, lsize, psize, obj_size, OUT obj);

  case Compress::Off:
    ASSERT(lsize == psize, "Logical (%zu) and physical (%zu) size don't match "
                           "even though compression is off?",
           lsize, psize);
    ASSERT(psize == bp.vdev[vdev_index].asize, "Phyiscal (%zu) and allocated "
                                               "(%zu) size don't match even "
                                               "though compression is off?",
           psize, bp.vdev[vdev_index].asize);

    if (std::fread(obj, obj_size, 1, m_fp) != 1) {
      LOG("Failed to read uncompressed object!\n");
      return false;
    }

    return true;

  default:
    UNREACHABLE("Unhandled compression: %u!", static_cast<u32>(bp.comp));
  }
}
