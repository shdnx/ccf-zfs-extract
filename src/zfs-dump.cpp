#include "zfs-dump.h"

// ---- DSL magic, you do not want to be here ----

static unsigned g_indent          = 0;
static bool     g_suppress_indent = false;

#define INDENT_LENGTH 4
#define FMT64 "0x%016lx"
#define FMT32 "0x%08x"
#define FMT16 "0x%04hx"
#define FMT8 "0x%02hhx"

struct IndentRAII {
  int  m_offset;
  bool m_flag = true;

  IndentRAII(int indent_offset = 1) : m_offset{indent_offset} {
    g_indent += m_offset;
  }

  ~IndentRAII() { g_indent -= m_offset; }
};

#define _INDENTED_IMPL(VAL, OBJNAME) \
  for (IndentRAII OBJNAME{(VAL)}; OBJNAME.m_flag; OBJNAME.m_flag = false)

#define INDENTED(VAL) _INDENTED_IMPL(VAL, CONCAT2(__indent, __COUNTER__))

#define PRINT_INDENT(fp)                                    \
  do {                                                      \
    if (!g_suppress_indent)                                 \
      std::fprintf(fp, "%*s", g_indent *INDENT_LENGTH, ""); \
    g_suppress_indent = false;                              \
  } while (0)

#define PRINT(fp, ...)             \
  do {                             \
    PRINT_INDENT(fp);              \
    std::fprintf(fp, __VA_ARGS__); \
  } while (0)

#define HEADER(fp, ...)   \
  PRINT(fp, __VA_ARGS__); \
  INDENTED(1)

#define _ONCE_IMPL(VARNAME) for (bool VARNAME = true; VARNAME; VARNAME = false)
#define ONCE() _ONCE_IMPL(CONCAT2(__once, __COUNTER__))

#define INLINE_HEADER(FP, ...) \
  PRINT(FP, __VA_ARGS__);      \
  g_suppress_indent = true;    \
  ONCE()

static constexpr const char *getDefaultFormat(size_t fieldSize) {
#define PREFIX "%-15s = "
#define SUFFIX "\n"

  switch (fieldSize) {
  case 8:
    return PREFIX FMT64 SUFFIX;
  case 4:
    return PREFIX FMT32 SUFFIX;
  case 2:
    return PREFIX FMT16 SUFFIX;
  case 1:
    return PREFIX FMT8 SUFFIX;
  default:
    return nullptr;
  }

#undef SUFFIX
#undef PREFIX
}

#define DUMP(FP, OBJ, FIELD)                                      \
  do {                                                            \
    const auto __val = (OBJ).FIELD; /* to get around bitfields */ \
    PRINT((FP), getDefaultFormat(sizeof(__val)), #FIELD, __val);  \
  } while (0)

template <typename TObj> struct DumpCtx {
  FILE *      fp;
  const TObj *obj;
  bool        run = true;

  explicit DumpCtx(FILE *fp_, const TObj *obj_) : fp{fp_}, obj{obj_} {}
};

template <typename TObj>
static inline DumpCtx<TObj> make_dump_ctx(FILE *fp, const TObj &obj) {
  return DumpCtx<TObj>{fp, &obj};
}

#define DUMP_OBJECT(FP, OBJ)                                       \
  for (auto __dumpctx = make_dump_ctx((FP), (OBJ)); __dumpctx.run; \
       __dumpctx.run  = false)

#define OBJECT_HEADER(FP, OBJ, TITLE) \
  HEADER((FP), TITLE)                 \
  DUMP_OBJECT((FP), (OBJ))

#define DUMP_FIELD(FIELDNAME) DUMP(__dumpctx.fp, *__dumpctx.obj, FIELDNAME)

// ----- end DSL magic ------

void Dump::dva(FILE *fp, const Dva &dva) {
  HEADER(fp, "DVA <0x%x:0x%lx:0x%x>\n", dva.vdev, dva.offset, dva.asize) {}
}

void Dump::blkptr(FILE *fp, const Blkptr &blkptr) {
  HEADER(fp, "BLKPTR <L:0x%lx, P:0x%lx>:\n", blkptr.getLogicalSize(),
         blkptr.getPhysicalSize()) {
    DUMP_OBJECT(fp, blkptr) {
      DUMP_FIELD(type);
      DUMP_FIELD(comp);
      DUMP_FIELD(cksum);
      DUMP_FIELD(fill);
      DUMP_FIELD(birth_txg);

      for (size_t i = 0; i < 3; i++) {
        INLINE_HEADER(fp, "vdev[%zu]: ", i) { Dump::dva(fp, blkptr.vdev[i]); }
      }
    }
  }
}

void Dump::uberblock(FILE *fp, const Uberblock &ub) {
  HEADER(fp, "Uberblock 0x%lx:\n", ub.txg) {
    DUMP_OBJECT(fp, ub) {
      DUMP_FIELD(timestamp);
      DUMP_FIELD(version);
    }

    INLINE_HEADER(fp, "rootbp: ") { Dump::blkptr(fp, ub.rootbp); }
  }
}

void Dump::dnode(FILE *fp, const DNode &dnode) {
  OBJECT_HEADER(fp, dnode, "DNode:\n") {
    DUMP_FIELD(phys_comp);
    DUMP_FIELD(checksum);
    DUMP_FIELD(nblkptr);
    DUMP_FIELD(nlevels);
    DUMP_FIELD(type);
    DUMP_FIELD(bonuslen);
    DUMP_FIELD(flags);
    DUMP_FIELD(max_block_id);
    DUMP_FIELD(secphys_used);

    for (size_t i = 0; i < dnode.nblkptr; i++) {
      INLINE_HEADER(fp, "bps[%zu]: ", i) { Dump::blkptr(fp, dnode.bps[i]); }
    }
  }
}

void Dump::objset(FILE *fp, const ObjSet &objset) {
  HEADER(fp, "ObjSet:\n") {
    DUMP_OBJECT(fp, objset) { DUMP_FIELD(type); }

    INLINE_HEADER(fp, "metadnode: ") { Dump::dnode(fp, objset.metadnode); }
  }
}
