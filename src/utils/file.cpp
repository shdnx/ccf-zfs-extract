#include "utils/file.h"
#include "utils/common.h"

const char *File::getModeStr(File::Mode m) noexcept {
  switch (m) {
  case Read:
    return "r";
  case Write:
    return "w";
  case Update:
    return "r+";
  case Overwrite:
    return "w+";
  case Append:
    return "a";
  case UpdateAppend:
    return "a+";
  default:
    UNREACHABLE("Unhandled File::Mode: %d!", m);
  }
}
