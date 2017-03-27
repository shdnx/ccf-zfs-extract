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

  case 2:
    return std::fseek(fp, -static_cast<long>(VDEV_LABEL_SIZE), SEEK_END) == 0;

  case 3:
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

  return ub->isValid();
}

// blatantly stolen from byteorder.h in linux-on-zfs source tree
#define BE_IN8(xa) *((u8 *)(xa))
#define BE_IN16(xa) (((u16)BE_IN8(xa) << 8) | BE_IN8((u8 *)(xa) + 1))
#define BE_IN32(xa) (((u32)BE_IN16(xa) << 16) | BE_IN16((u8 *)(xa) + 2))
#define BE_IN64(xa) (((u64)BE_IN32(xa) << 32) | BE_IN32((u8 *)(xa) + 4))

static bool readLZ4CompressedData(FILE *fp, size_t lsize, size_t psize,
                                  OUT void *data, OUT int *result) {
  ASSERT(psize % SECTOR_SIZE == 0, "Non-sector aligned physical size: %zu",
         psize);

  std::unique_ptr<char[]> ibuffer{new char[psize]};

  const size_t nread = std::fread(ibuffer.get(), sizeof(char), psize, fp);
  if (nread != psize) {
    LOG("Failed to read compressed object of psize = %lx, "
        "could only read: %zu\n",
        psize, nread);
    return false;
  }

  u32 *     ibuffer_raw     = reinterpret_cast<u32 *>(ibuffer.get());
  const u32 compressed_size = BE_IN32(ibuffer_raw);

  if (lsize <= compressed_size + sizeof(compressed_size)) {
    LOG("Cannot LZ4 decompress: lnvalid logical size %zu: lower than the "
        "compressed size %u\n",
        lsize, compressed_size);
    return false;
  }

  const int decompress_result = LZ4_decompress_safe(
      reinterpret_cast<const char *>(&ibuffer_raw[1]),
      OUT reinterpret_cast<char *>(data), static_cast<int>(compressed_size),
      static_cast<int>(lsize));

  LOG("LZ4_decompress_safe => %d (compressed_size = %u, lsize = %zu, psize = "
      "%zu)\n",
      decompress_result, compressed_size, lsize, psize);

  OUT *result = decompress_result;
  return true;
}

static Compress getEffectiveCompression(Compress comp) {
  switch (comp) {
  case Compress::On:
    return Compress::Default;

  case Compress::Inherit:
    LOG("Warning: treating Compress::Inherit as Compress::Default!\n");
    return Compress::Default;

  default:
    return comp;
  }
}

bool ZPool::readRawData(const Blkptr &bp, u32 dva_index, OUT void *data) {
  ASSERT(bp.isValid(), "Cannot resolve invalid blkptr!");
  ASSERT(bp.endian == Endian::Little, "Cannot handle big endian blkptr!");

  const size_t lsize = bp.getLogicalSize();
  const size_t psize = bp.getPhysicalSize();

  const Dva &dva = bp.dva[dva_index];
  ASSERT(dva.isValid(), "Cannot resolve invalid DVA at index %u!", dva_index);
  ASSERT(!dva.gang_block, "Don't know how to resolve a gangblock DVA!");

  const u64    addr  = dva.getAddress();
  const size_t asize = dva.getAllocatedSize();

  LOG("Reading %zu logical (%zu physical) bytes from DVA: ", lsize, psize);
  dva.dump(stderr);

  if (std::fseek(m_fp, addr, SEEK_SET) != 0) {
    LOG("Seek failed!\n");
    return false;
  }

  const Compress comp = getEffectiveCompression(bp.comp);
  switch (comp) {
  case Compress::LZ4:
    int decompress_result;
    return readLZ4CompressedData(m_fp, lsize, psize, OUT data,
                                 OUT & decompress_result);

  case Compress::Off:
    ASSERT(lsize == psize && lsize == asize, "Mismatch between logical (%zu), "
                                             "physical (%zu) and allocated "
                                             "(%zu) sizes "
                                             "even though compression is off!",
           lsize, psize, asize);

    if (std::fread(data, sizeof(char), lsize, m_fp) != lsize) {
      LOG("Failed to read uncompressed data!\n");
      return false;
    }

    return true;

  default:
    UNREACHABLE("Unhandled compression: %u!", static_cast<u32>(comp));
  }
}
