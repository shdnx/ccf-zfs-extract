#include "zfs/byteorder.h"
#include "utils/log.h"

namespace zfs {

void byteswap_uint64_array(void *vbuf, size_t size) {
  u64 *  buf   = static_cast<u64 *>(vbuf);
  size_t count = size >> 3;

  ASSERT0((size & 7) == 0);

  for (size_t i = 0; i < count; i++)
    buf[i]      = BSWAP_64(buf[i]);
}

void byteswap_uint32_array(void *vbuf, size_t size) {
  u32 *  buf   = static_cast<u32 *>(vbuf);
  size_t count = size >> 2;

  ASSERT0((size & 3) == 0);

  for (size_t i = 0; i < count; i++)
    buf[i]      = BSWAP_32(buf[i]);
}

void byteswap_uint16_array(void *vbuf, size_t size) {
  u16 *  buf   = static_cast<u16 *>(vbuf);
  size_t count = size >> 1;

  ASSERT0((size & 1) == 0);

  for (size_t i = 0; i < count; i++)
    buf[i]      = BSWAP_16(buf[i]);
}

void byteswap_uint8_array(void *vbuf, size_t size) {}

void byteswap_obj(void *obj, size_t obj_size) {
  ASSERT(obj_size % sizeof(u64) == 0, "Object size not aligned to u64!");

  LOG("Note: byteorder-correcting object %p of size %zu!\n", obj, obj_size);
  byteswap_uint64_array(obj, obj_size);
}

} // end namespace zfs
