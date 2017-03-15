#include <cstdio>
#include <memory>

#include "common.h"
#include "zfs.h"
#include "zfs-dump.h"

int main(int argc, const char **argv) {
  CRITICAL(argc > 1, "Usage: %s <zpool-file-path>\n", argv[0]);

  const char *path = argv[1];
  FILE *fp = std::fopen(path, "rb");
  CRITICAL(fp, "Unable to open zpool file %s!\n", path);

  //TODO: read all vdev labels for redundancy
  for (int i = 0; i < 128; i++) {
    std::fseek(fp, (128 + i)*1024, SEEK_SET);

    Uberblock ub;
    if (ub.readFrom(fp)) {
      std::fprintf(stdout, "Found Uberblock @ %08lx\n", (size_t)(ftell(fp) - sizeof(Uberblock)));
      Dump::uberblock(stdout, ub);
    }
  }

  std::fclose(fp);
  return 0;
}
