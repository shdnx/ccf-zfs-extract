#include <cstdio>
#include <type_traits>

#include "zfs/physical.h"

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

#define FMT_PREFIX "%-20s = "
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

template <typename T>
static inline void printFieldImpl(std::FILE *fp, const char *fieldName,
                                  T fieldValue, std::true_type) {
  PRINT(fp, FMT_PREFIX "%lu (0x%lx)" FMT_SUFFIX, fieldName,
        static_cast<u64>(fieldValue), static_cast<u64>(fieldValue));
}

template <typename T>
static inline void printFieldImpl(std::FILE *fp, const char *fieldName,
                                  T fieldValue, std::false_type) {
  PRINT(fp, FieldFormat<std::decay_t<T>>::value, fieldName, fieldValue);
}

template <typename T>
static inline void printField(std::FILE *fp, const char *fieldName,
                              T fieldValue) {
  printFieldImpl(fp, fieldName, fieldValue,
                 std::integral_constant<bool, std::is_enum<T>::value>{});
}

#undef FMT_SUFFIX
#undef FMT_PREFIX

#define DUMP(FP, OBJ, FIELD)                                      \
  do {                                                            \
    const auto __val = (OBJ).FIELD; /* to get around bitfields */ \
    printField((FP), #FIELD, __val);                              \
  } while (0)

template <typename TObj>
struct DumpObjCtx {
  std::FILE * fp;
  const TObj *obj;
  bool        run = true;

  explicit DumpObjCtx(std::FILE *fp_, const TObj *obj_) : fp{fp_}, obj{obj_} {}
};

template <typename TObj>
static inline DumpObjCtx<TObj> make_obj_ctx(std::FILE *fp, const TObj &obj) {
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
static void checkValid(std::FILE *fp, const TObj *obj) {
  if (!obj->isValid()) {
    PRINT(fp, "!! WARNING: failed validation: %s\n", TObj::validation_expr);
  }
}

namespace zfs {
namespace physical {

void Dva::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DVA: invalid\n");
    return;
  }

  HEADER(fp, "DVA <0x%x:0x%lx:0x%x> = 0x%016lx", vdev, offset, asize,
         getAddress()) {
    checkValid(fp, this);
  }
}

void Blkptr::dump(std::FILE *fp, DumpFlags flags) const {
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
      INLINE_HEADER(fp, "dva[%zu]: ", i) { dva[i].dump(fp, flags); }
    }

    checkValid(fp, this);
  }
}

void Uberblock::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "Uberblock: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "Uberblock 0x%lx:", txg) {
    DUMP_FIELD(timestamp);
    DUMP_FIELD(spa_version);

    INLINE_HEADER(fp, "rootbp: ") { rootbp.dump(fp, flags); }

    checkValid(fp, this);
  }
}

void DNode::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DNode: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "DNode <%s>:", getDNodeTypeAsString(type)) {
    DUMP_FIELD(type);
    DUMP_FIELD(bonustype);
    DUMP_FIELD(bonuslen);

    DUMP_FIELD(phys_comp);
    DUMP_FIELD(checksum);
    DUMP_FIELD(indblkshift);
    DUMP_FIELD(datablksecsize);
    DUMP_FIELD(nblkptr);
    DUMP_FIELD(nlevels);

    DUMP_FIELD(flags);
    DUMP_FIELD(max_block_id);
    DUMP_FIELD(secphys_used);

    for (size_t i = 0; i < nblkptr; i++) {
      INLINE_HEADER(fp, "bps[%zu]: ", i) { bps[i].dump(fp, flags); }
    }

    checkValid(fp, this);
  }
}

void ObjSet::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "ObjSet: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "ObjSet:") {
    DUMP_FIELD(type);

    INLINE_HEADER(fp, "metadnode: ") { metadnode.dump(fp, flags); }

    checkValid(fp, this);
  }
}

void MZapEntry::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapEntry: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "MZapEntry <%s = 0x%lx>", name, value) {}
}

void MZapHeader::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapHeader: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "MZapHeader:") {
    DUMP_FIELD(salt);
    DUMP_FIELD(normflags);
  }
}

void DSLDir::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DSLDir: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "DSLDir:") {
    DUMP_FIELD(creation_time);
    DUMP_FIELD(head_dataset_obj);
    DUMP_FIELD(parent_obj);
    DUMP_FIELD(origin_obj);
    DUMP_FIELD(child_dir_zapobj);
    DUMP_FIELD(used_bytes);
    DUMP_FIELD(compressed_bytes);
    DUMP_FIELD(uncompressed_bytes);
    DUMP_FIELD(flags);
  }
}

void DSLDataSet::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "DSLDataSet: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "DSLDataSet:") {
    DUMP_FIELD(dir_obj);
    DUMP_FIELD(prev_snap_obj);
    DUMP_FIELD(prev_snap_txg);
    DUMP_FIELD(next_snap_obj);
    DUMP_FIELD(snapnames_zapobj);
    DUMP_FIELD(nchildren);

    DUMP_FIELD(creation_time);
    DUMP_FIELD(creation_txg);

    DUMP_FIELD(referenced_bytes);
    DUMP_FIELD(compressed_bytes);
    DUMP_FIELD(uncompressed_bytes);
    DUMP_FIELD(unique_bytes);

    DUMP_FIELD(guid);
    DUMP_FIELD(flags);

    INLINE_HEADER(fp, "bp: ") { bp.dump(fp, flags); }
  }
}

void ZNodeTime::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "ZNodeTime: invalid\n");
    return;
  }

  PRINT(fp, "ZNodeTime <%lu.%lu>\n", seconds, nanoseconds);
}

void ZNode::dump(std::FILE *fp, DumpFlags flags) const {
  if (!isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "ZNode: invalid\n");
    return;
  }

  OBJECT_HEADER(fp, *this, "ZNode:") {
    DUMP_FIELD(gen_txg);
    DUMP_FIELD(mode);
    DUMP_FIELD(size);
    DUMP_FIELD(parent_obj);
    DUMP_FIELD(links);
    DUMP_FIELD(flags);
    DUMP_FIELD(uid);
    DUMP_FIELD(gid);

    INLINE_HEADER(fp, "time_created: ") { time_created.dump(fp, flags); }
    INLINE_HEADER(fp, "time_modified: ") { time_modified.dump(fp, flags); }
    INLINE_HEADER(fp, "time_changed: ") { time_changed.dump(fp, flags); }
    INLINE_HEADER(fp, "time_accessed: ") { time_accessed.dump(fp, flags); }
  }
}

} // end namespace physical

void MZapBlockPtr::dump(std::FILE *fp, DumpFlags flags) const {
  if (!header()->isValid() && !flag_isset(flags, DumpFlags::AllowInvalid)) {
    PRINT(fp, "MZapBlock: invalid\n");
    return;
  }

  HEADER(fp, "MZapBlock:") {
    INLINE_HEADER(fp, "header: ") {
      if (const physical::MZapHeader *hdr = header()) {
        hdr->dump(fp, flags);
      } else {
        PRINT(fp, "(nullptr)\n");
      }
    }

    const size_t nentries = numEntries();
    HEADER(fp, "entries[%zu]: ", nentries) {
      for (size_t i = 0; i < nentries; i++) {
        INLINE_HEADER(fp, "[%zu]: ", i) { (*this)[i].dump(fp, flags); }
      }
    }
  }
}

} // end namespace zfs
