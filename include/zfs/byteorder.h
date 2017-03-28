#pragma once

#include "utils/common.h"

namespace zfs {

// stolen from zfs-on-linux byteorder.h

/*
 * Macros to reverse byte order
 */
#define BSWAP_8(x) ((x)&0xff)
#define BSWAP_16(x) ((BSWAP_8(x) << 8) | BSWAP_8((x) >> 8))
#define BSWAP_32(x) ((BSWAP_16(x) << 16) | BSWAP_16((x) >> 16))
#define BSWAP_64(x) ((BSWAP_32(x) << 32) | BSWAP_32((x) >> 32))

#define BMASK_8(x) ((x)&0xff)
#define BMASK_16(x) ((x)&0xffff)
#define BMASK_32(x) ((x)&0xffffffff)
#define BMASK_64(x) (x)

/*
 * Macros to convert from a specific byte order to/from native byte order
 */
#ifdef _BIG_ENDIAN
#define BE_8(x) BMASK_8(x)
#define BE_16(x) BMASK_16(x)
#define BE_32(x) BMASK_32(x)
#define BE_64(x) BMASK_64(x)
#define LE_8(x) BSWAP_8(x)
#define LE_16(x) BSWAP_16(x)
#define LE_32(x) BSWAP_32(x)
#define LE_64(x) BSWAP_64(x)
#else
#define LE_8(x) BMASK_8(x)
#define LE_16(x) BMASK_16(x)
#define LE_32(x) BMASK_32(x)
#define LE_64(x) BMASK_64(x)
#define BE_8(x) BSWAP_8(x)
#define BE_16(x) BSWAP_16(x)
#define BE_32(x) BSWAP_32(x)
#define BE_64(x) BSWAP_64(x)
#endif

void byteswap_uint64_array(void *vbuf, std::size_t size);
void byteswap_uint32_array(void *vbuf, std::size_t size);
void byteswap_uint16_array(void *vbuf, std::size_t size);
void byteswap_uint8_array(void *vbuf, std::size_t size);

enum class Endian : bool { Little = 1, Big = 0 };

#ifdef _BIG_ENDIAN
#define SYSTEM_ENDIAN Endian::Big
#else
#define SYSTEM_ENDIAN Endian::Little
#endif

} // end namespace zfs
