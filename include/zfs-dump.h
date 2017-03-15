#pragma once

#include <cstdio>

#include "common.h"
#include "zfs.h"

struct Dump {
  static void uberblock(FILE *fp, const Uberblock &ub);
  static void blkptr(FILE *fp, const Blkptr &blkptr);
  static void blkptr_props(FILE *fp, const Props &props);
  static void dva(FILE *fp, const Dva &dva);
};