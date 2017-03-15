#include <cstdio>

#include "zfs.h"

bool Uberblock::readFrom(FILE *fp) {
  size_t nread = fread(this, sizeof(Uberblock), 1, fp);
  CRITICAL(nread == 1, "Failed to read uberblock!\n");

  if (magic != UB_MAGIC)
    return false;

  return true;
}

std::unique_ptr<Uberblock> Uberblock::read(FILE *fp) {
  auto ub = std::make_unique<Uberblock>();
  if (!ub->readFrom(fp))
    return nullptr;

  return std::move(ub);
}