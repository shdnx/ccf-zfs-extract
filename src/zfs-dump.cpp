#include <cstdio>
#include <type_traits>

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
  PRINT(fp, "\n");        \
  INDENTED(1)

#define _ONCE_IMPL(VARNAME) for (bool VARNAME = true; VARNAME; VARNAME = false)
#define ONCE() _ONCE_IMPL(CONCAT2(__once, __COUNTER__))

#define INLINE_HEADER(FP, ...) \
  PRINT(FP, __VA_ARGS__);      \
  g_suppress_indent = true;    \
  ONCE()

template <typename T>
struct FieldFormat;

#define FMT_PREFIX "%-15s = "
#define FMT_SUFFIX "\n"

#define FIELD_FORMAT(TYPE, VALUE_FMT)                                     \
  template <>                                                             \
  struct FieldFormat<TYPE> {                                              \
    static constexpr const char *value = FMT_PREFIX VALUE_FMT FMT_SUFFIX; \
  }

FIELD_FORMAT(u64, FMT64);
FIELD_FORMAT(u32, FMT32);
FIELD_FORMAT(u16, FMT16);
FIELD_FORMAT(u8, FMT8);
FIELD_FORMAT(bool, "%d");
FIELD_FORMAT(char *, "%s");

#undef FMT_SUFFIX
#undef FMT_PREFIX

template <typename T, typename V = void>
struct UnwrappedEnum;

template <typename T>
struct UnwrappedEnum<T,
                     std::enable_if_t<std::is_enum<T>::value, std::true_type>> {
  using type = std::underlying_type_t<T>;
};

template <typename T>
struct UnwrappedEnum<T, std::true_type> {
  using type = T;
};

template <typename T>
constexpr const char *getFieldFormat(T val) {
  using D = std::decay_t<T>;
  return FieldFormat<typename UnwrappedEnum<D>::type>::value;
}

#define DUMP(FP, OBJ, FIELD)                                      \
  do {                                                            \
    const auto __val = (OBJ).FIELD; /* to get around bitfields */ \
    PRINT((FP), getFieldFormat(__val), #FIELD, __val);            \
  } while (0)

template <typename TObj>
struct DumpObjCtx {
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

#define OBJECT_HEADER(FP, OBJ, ...) \
  HEADER((FP), __VA_ARGS__)         \
  DUMP_OBJECT((FP), (OBJ))

#define DUMP_FIELD(FIELDNAME) DUMP(__dumpctx.fp, *__dumpctx.obj, FIELDNAME)

// ----- end DSL magic ------

template <typename TObj>
static void checkValid(FILE *fp, const TObj *obj) {
  if (!obj->isValid()) {
    PRINT(fp, "!! WARNING: failed validation: %s\n", TObj::validation_expr);
  }
}

void Dva::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DVA: invalid\n");
    return;
  }

  HEADER(fp, "DVA <0x%x:0x%lx:0x%x> = 0x%016lx", vdev, offset, asize,
         getAddress()) {
    checkValid(fp, this);
  }
}

void Blkptr::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "BLKPTR: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "BLKPTR <L:0x%lx, P:0x%lx>:", getLogicalSize(),
                getPhysicalSize()) {
    DUMP_FIELD(type);
    DUMP_FIELD(comp);
    DUMP_FIELD(endian);
    DUMP_FIELD(cksum);
    DUMP_FIELD(fill);
    DUMP_FIELD(birth_txg);

    for (size_t i = 0; i < 3; i++) {
      INLINE_HEADER(fp, "vdev[%zu]: ", i) { vdev[i].dump(fp); }
    }

    checkValid(fp, this);
  }
}

void Uberblock::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "Uberblock: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "Uberblock 0x%lx:", txg) {
    DUMP_FIELD(timestamp);
    DUMP_FIELD(spa_version);

    INLINE_HEADER(fp, "rootbp: ") { rootbp.dump(fp); }

    checkValid(fp, this);
  }
}

void DNode::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DNode: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "DNode:") {
    DUMP_FIELD(phys_comp);
    DUMP_FIELD(checksum);
    DUMP_FIELD(indblkshift);
    DUMP_FIELD(data_blk_size_secs);
    DUMP_FIELD(nblkptr);
    DUMP_FIELD(nlevels);
    DUMP_FIELD(type);
    DUMP_FIELD(bonuslen);
    DUMP_FIELD(bonustype);
    DUMP_FIELD(flags);
    DUMP_FIELD(max_block_id);
    DUMP_FIELD(secphys_used);

    for (size_t i = 0; i < nblkptr; i++) {
      INLINE_HEADER(fp, "bps[%zu]: ", i) { bps[i].dump(fp); }
    }

    checkValid(fp, this);
  }
}

void ObjSet::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "ObjSet: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "ObjSet:") {
    DUMP_FIELD(type);

    INLINE_HEADER(fp, "metadnode: ") { metadnode.dump(fp); }

    checkValid(fp, this);
  }
}

void MZapEntry::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapEntry: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "MZapEntry <%s = 0x%lx>", name, value) {}
}

void MZapHeader::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapHeader: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "MZapHeader:") {
    DUMP_FIELD(salt);
    DUMP_FIELD(normflags);
  }
}

void MZapBlock::dump(FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapBlock: invalid\n");
    return;
  }

  HEADER(fp, "MZapBlock:") {
    INLINE_HEADER(fp, "header: ") { header.dump(fp, flags); }

    const size_t nentries = entries.size();
    HEADER(fp, "entries[%zu]: ", nentries) {
      for (const i = 0; i < nentries; i++) {
        INLINE_HEADER(fp, "[%zu]: ", i) { entries[i].dump(fp, flags); }
      }
    }
  }
}
