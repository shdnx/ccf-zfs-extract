#include <cstdio>

#include "zfs-objects.h"

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

  explicit IndentRAII(int indent_offset = 1) : m_offset{indent_offset} {
    g_indent += m_offset;
  }

  ~IndentRAII() { g_indent -= m_offset; }
};

#define _INDENTED_IMPL(VAL, OBJNAME) \
  for (IndentRAII OBJNAME{(VAL)}; OBJNAME.m_flag; OBJNAME.m_flag = false)

#define INDENTED(VAL) _INDENTED_IMPL(VAL, CONCAT2(__indent, __COUNTER__))

static void print_indent(FILE *fp) {
  if (!g_suppress_indent)
    std::fprintf(fp, "%*s", g_indent * INDENT_LENGTH, "");
  g_suppress_indent = false;
}

#define PRINT(fp, ...)             \
  do {                             \
    print_indent(fp);              \
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

static constexpr const char *getFieldFormat(size_t fieldSize) {
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
    PRINT((FP), getFieldFormat(sizeof(__val)), #FIELD, __val);    \
  } while (0)

template <typename TObj> struct DumpObjCtx {
  FILE *      fp;
  const TObj *obj;
  bool        run = true;

  explicit DumpObjCtx(FILE *fp_, const TObj *obj_) : fp{fp_}, obj{obj_} {}
};

template <typename TObj>
static inline DumpObjCtx<TObj> make_obj_ctx(FILE *fp, const TObj &obj) {
  return DumpObjCtx<TObj>{fp, &obj};
}

#define DUMP_OBJECT(FP, OBJ)                                      \
  for (auto __dumpctx = make_obj_ctx((FP), (OBJ)); __dumpctx.run; \
       __dumpctx.run  = false)

#define OBJECT_HEADER(FP, OBJ, TITLE) \
  HEADER((FP), TITLE)                 \
  DUMP_OBJECT((FP), (OBJ))

#define DUMP_FIELD(FIELDNAME) DUMP(__dumpctx.fp, *__dumpctx.obj, FIELDNAME)

// ----- end DSL magic ------

template <typename TObj> static void checkValid(FILE *fp, const TObj *obj) {
  if (!obj->validate()) {
    PRINT(fp, "!! WARNING: failed validation: %s\n", TObj::validation_expr);
  }
}

void Dva::dump(FILE *fp) const {
  HEADER(fp, "DVA <0x%x:0x%lx:0x%x>\n", vdev, offset, asize) {
    checkValid(fp, this);
  }
}

void Blkptr::dump(FILE *fp) const {
  HEADER(fp, "BLKPTR <L:0x%lx, P:0x%lx>:\n", getLogicalSize(),
         getPhysicalSize()) {
    DUMP_OBJECT(fp, *this) {
      DUMP_FIELD(type);
      DUMP_FIELD(comp);
      DUMP_FIELD(cksum);
      DUMP_FIELD(fill);
      DUMP_FIELD(birth_txg);

      for (size_t i = 0; i < 3; i++) {
        INLINE_HEADER(fp, "vdev[%zu]: ", i) { vdev[i].dump(fp); }
      }
    }

    checkValid(fp, this);
  }
}

void Uberblock::dump(FILE *fp) const {
  HEADER(fp, "Uberblock 0x%lx:\n", txg) {
    DUMP_OBJECT(fp, *this) {
      DUMP_FIELD(timestamp);
      DUMP_FIELD(spa_version);
    }

    INLINE_HEADER(fp, "rootbp: ") { rootbp.dump(fp); }

    checkValid(fp, this);
  }
}

void DNode::dump(FILE *fp) const {
  OBJECT_HEADER(fp, *this, "DNode:\n") {
    DUMP_FIELD(phys_comp);
    DUMP_FIELD(checksum);
    DUMP_FIELD(nblkptr);
    DUMP_FIELD(nlevels);
    DUMP_FIELD(type);
    DUMP_FIELD(bonuslen);
    DUMP_FIELD(flags);
    DUMP_FIELD(max_block_id);
    DUMP_FIELD(secphys_used);

    for (size_t i = 0; i < nblkptr; i++) {
      INLINE_HEADER(fp, "bps[%zu]: ", i) { bps[i].dump(fp); }
    }

    checkValid(fp, this);
  }
}

void ObjSet::dump(FILE *fp) const {
  HEADER(fp, "ObjSet:\n") {
    DUMP_OBJECT(fp, *this) { DUMP_FIELD(type); }

    INLINE_HEADER(fp, "metadnode: ") { metadnode.dump(fp); }

    checkValid(fp, this);
  }
}
