#pragma once

#include <cstdio>

#include "zfs.h"

struct Dump {
  static void dnode(FILE *fp, const DNode &dnode);
  static void objset(FILE *fp, const ObjSet &objset);
  static void dva(FILE *fp, const Dva &dva);
  static void blkptr(FILE *fp, const Blkptr &blkptr);
  static void uberblock(FILE *fp, const Uberblock &ub);
};
