#include "zfs-dump.h"

static unsigned g_indent = 0;

struct IndentRAII {
  int m_offset;

  IndentRAII(int indent_offset = 1) : m_offset{indent_offset} {
    g_indent += m_offset;
  }

  ~IndentRAII() {
    g_indent -= m_offset;
  }
};

#define PRINT(fp, ...) \
  do { \
    fprintf(fp, "%*s", g_indent * 4); \
    fprintf(fp, __VA_ARGS__); \
  } while (0)

void Dump::dva(FILE *fp, const Dva &dva) {
  PRINT(fp, "vdev = %08x\n", dva.vdev);
  PRINT(fp, "grid = %hhx\n", dva.grid);
  PRINT(fp, "asize = %08x\n", dva.asize);
  PRINT(fp, "offset = %016lx\n", dva.offset);
  PRINT(fp, "gang = %d\n", dva.gang_block);
}

void Dump::blkptr(FILE *fp, const Blkptr &blkptr) {
  PRINT(fp, "BLKPTR:\n");
  IndentRAII x{};

  for (size_t i = 0; i < 3; i++) {
    PRINT(fp, "vdev[%zu]:\n", i);

    IndentRAII x{};
    Dump::dva(fp, blkptr.vdev[i]);
  }

  // TODO: props, etc.
}

void Dump::uberblock(FILE *fp, const Uberblock &ub) {
  PRINT(fp, "Uberblock:\n");
  IndentRAII x{};

  PRINT(fp, "TXG: %08lx\n", ub.txg);
  PRINT(fp, "timestamp: %lu\n", ub.timestamp);
  PRINT(fp, "version: %lu\n", ub.version);

  Dump::blkptr(fp, ub.blkptr);
}

/*Found Uberblock @ 00021000
TXG:    4, timestamp: 1487966644, version: 5000
BLKPTR: birth: 4
vdev1: 0, grid: 0, asize: 1
offset: 00004e00
vdev1: 0, grid: 0, asize: 1
offset: 0c004e00
vdev1: 0, grid: 0, asize: 1
offset: 18002600
E: 1
lvl: 00
type: 0b
cksum: 07
comp: 03
psize: 0f00
lsize: 0000
*/