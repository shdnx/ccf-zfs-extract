#include "zfs-dump.h"

// ---- DSL magic, you do not want to be here ----

static unsigned g_indent = 0;

#define INDENT_LENGTH 4

struct IndentRAII {
  int  m_offset;
  bool m_flag = true;

  IndentRAII(int indent_offset = 1) : m_offset{indent_offset} {
    g_indent += m_offset;
  }

  ~IndentRAII() { g_indent -= m_offset; }
};

#define _INDENTED_IMPL(OBJNAME) \
  for (IndentRAII OBJNAME; OBJNAME.m_flag; OBJNAME.m_flag = false)

#define INDENTED _INDENTED_IMPL(CONCAT2(__indent, __COUNTER__))

#define PRINT(fp, ...)                                    \
  do {                                                    \
    std::fprintf(fp, "%*s", g_indent *INDENT_LENGTH, ""); \
    std::fprintf(fp, __VA_ARGS__);                        \
  } while (0)

#define HEADER(fp, ...)   \
  PRINT(fp, __VA_ARGS__); \
  INDENTED

static constexpr const char *getDefaultFormat(size_t fieldSize) {
#define PREFIX "%-15s = "
#define SUFFIX "\n"

  switch (fieldSize) {
  case 8:
    return PREFIX "0x%016lx" SUFFIX;
  case 4:
    return PREFIX "0x%08x" SUFFIX;
  case 2:
    return PREFIX "0x%04hx" SUFFIX;
  case 1:
    return PREFIX "%d" SUFFIX;
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
  OBJECT_HEADER(fp, dva, "DVA:\n") {
    DUMP_FIELD(vdev);
    DUMP_FIELD(offset);
    DUMP_FIELD(asize);
  }
}

void Dump::blkptr_props(FILE *fp, const BlkptrProps &props) {
  OBJECT_HEADER(fp, props, "BLKPTR Props:\n") {
    DUMP_FIELD(lsize);
    DUMP_FIELD(psize);
    DUMP_FIELD(comp);
    DUMP_FIELD(cksum);
    DUMP_FIELD(type);
    DUMP_FIELD(endian);
  }
}

void Dump::blkptr(FILE *fp, const Blkptr &blkptr) {
  HEADER(fp, "BLKPTR:\n") {
    DUMP_OBJECT(fp, blkptr) {
      DUMP_FIELD(fill);
      DUMP_FIELD(birth_txg);
    }

    Dump::blkptr_props(fp, blkptr.props);

    for (size_t i = 0; i < 3; i++) {
      HEADER(fp, "vdev[%zu]:\n", i) { Dump::dva(fp, blkptr.vdev[i]); }
    }
  }
}

void Dump::uberblock(FILE *fp, const Uberblock &ub) {
  OBJECT_HEADER(fp, ub, "Uberblock:\n") {
    DUMP_FIELD(txg);
    DUMP_FIELD(timestamp);
    DUMP_FIELD(version);

    Dump::blkptr(fp, ub.rootbp);
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
  }
}