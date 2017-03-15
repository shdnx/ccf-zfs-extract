#include <cstdio>

#include "zfs.h"

bool Uberblock::readFrom(FILE *fp) {
  size_t nread = fread(this, sizeof(Uberblock), 1, fp);
  CRITICAL(nread == 1, "Failed to read uberblock!\n");

  if (magic != UB_MAGIC)
    return false;

  return true;
}