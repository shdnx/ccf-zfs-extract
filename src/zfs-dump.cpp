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

#define INDENTED_IMPL(CNT) \
  bool __flag##CNT = true; \
  for (IndentRAII __indent##CNT; __flag##CNT; __flag##CNT = false)

#define INDENTED INDENTED_IMPL(__COUNTER__)

#define PRINT(fp, ...) \
  do { \
    std::fprintf(fp, "%*s", g_indent * 4, ""); \
    std::fprintf(fp, __VA_ARGS__); \
  } while (0)

#define HEADER(fp, ...) PRINT(fp, __VA_ARGS__); INDENTED

void Dump::dva(FILE *fp, const Dva &dva) {
  PRINT(fp, "vdev = %08x\n", dva.vdev);
  PRINT(fp, "grid = %hhx\n", dva.grid);
  PRINT(fp, "asize = %08x\n", dva.asize);
  PRINT(fp, "offset = %016lx\n", dva.offset);
  PRINT(fp, "gang = %d\n", dva.gang_block);
}

void Dump::blkptr_props(FILE *fp, const Props &props) {
  HEADER(fp, "Props:\n") {
    PRINT(fp, "lsize = %04hx, psize = %04hx\n", props.lsize, props.psize);
    PRINT(fp, "comp = %02hhx\n", props.comp);
    PRINT(fp, "embedded = %d\n", props.embedded);
    PRINT(fp, "cksum = %02hhx\n", props.cksum);
    PRINT(fp, "type = %02hhx\n", props.type);
    PRINT(fp, "lvl = %02hhx\n", props.lvl);
    PRINT(fp, "encrypt = %d\n", props.encrypt);
    PRINT(fp, "dedup = %d\n", props.dedup);
    PRINT(fp, "byte_order = %d\n", props.byte_order);
  }
}

void Dump::blkptr(FILE *fp, const Blkptr &blkptr) {
  HEADER(fp, "BLKPTR:\n") {
    Dump::blkptr_props(fp, blkptr.props);

    for (size_t i = 0; i < 3; i++) {
      HEADER(fp, "vdev[%zu]:\n", i) {
        Dump::dva(fp, blkptr.vdev[i]);
      }
    }
  }
}

void Dump::uberblock(FILE *fp, const Uberblock &ub) {
  HEADER(fp, "Uberblock:\n") {
    PRINT(fp, "TXG: %08lx\n", ub.txg);
    PRINT(fp, "timestamp: %lu\n", ub.timestamp);
    PRINT(fp, "version: %lu\n", ub.version);

    Dump::blkptr(fp, ub.blkptr);
  }
}
