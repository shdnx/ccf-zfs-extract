#include <iostream>

#define UB_MAGIC 0x00BAB10C

#define	BMASK_8(x)	((x) & 0xff)
#define	BMASK_16(x)	((x) & 0xffff)
#define BMASK_24(x) ((x) & 0xffffff)
#define	BMASK_32(x)	((x) & 0xffffffff)

class Dva {
public:
    uint64_t dva_word[2];

    uint32_t vdev() {
        return (uint32_t) BMASK_32(this->dva_word[0] >> 32);
    }

    uint8_t grid() {
        return (uint8_t) BMASK_8(this->dva_word[0] >> 24);
    }
    uint32_t asize() {
        return (uint32_t) BMASK_24(this->dva_word[0]);
    }
    uint64_t offset() {
        return (this->dva_word[0] << 1) >> 1;
    }
};



class Blkptr {
public:
    Dva   vdev[3];
    uint64_t props;
    uint64_t padding[3];
    uint64_t birth_txg;
    uint64_t fill;
    char     checksum[32];
};

class Uberblock {
public:
    uint64_t magic;
    uint64_t version;
    uint64_t txg;
    uint64_t guid_sum;
    uint64_t timestamp;
    Blkptr   blkptr;
};

#define VDA_GET_OFFSET(x) (((x.dva_word[1]) << 1) >> 1)

#define SECTOR_TO_ADDR(x) ((x << 9) + 0x400000)

void print_uber(Uberblock* ub) {
    fprintf(stdout, "TXG: %4lu, timestamp: %lu\n", ub->txg, ub->timestamp);
    fprintf(stdout, "BLKPTR: birth: %lu\n", ub->blkptr.birth_txg);
    fprintf(stdout, "vdev1: %x, grid: %x, asize: %x\n", ub->blkptr.vdev[0].vdev(), ub->blkptr.vdev[0].grid(), ub->blkptr.vdev[0].asize());
    fprintf(stdout, "offset: %08lx\n", SECTOR_TO_ADDR(ub->blkptr.vdev[0].offset()));
    fprintf(stdout, "vdev1: %x, grid: %x, asize: %x\n", ub->blkptr.vdev[1].vdev(), ub->blkptr.vdev[1].grid(), ub->blkptr.vdev[1].asize());
    fprintf(stdout, "offset: %08lx\n", SECTOR_TO_ADDR(VDA_GET_OFFSET(ub->blkptr.vdev[1])));
    fprintf(stdout, "vdev1: %x, grid: %x, asize: %x\n", ub->blkptr.vdev[2].vdev(), ub->blkptr.vdev[2].grid(), ub->blkptr.vdev[2].asize());
    fprintf(stdout, "offset: %08lx\n", SECTOR_TO_ADDR(VDA_GET_OFFSET(ub->blkptr.vdev[2])));
    fprintf(stdout, "OPTS: %lX\n", BMASK_16(ub->blkptr.props));
}

void read_uber(FILE *fp) {
    Uberblock* ub = (Uberblock*) malloc(sizeof(Uberblock));
    size_t bread = fread(ub, 1, sizeof(Uberblock), fp);
    if (bread < 1) {
        fprintf(stderr, "Read error");
        exit(1);
    }
    if (UB_MAGIC == ub->magic) {
        fprintf(stdout, "Found Uberblock @ %08x\n", (unsigned int)(ftell(fp) - sizeof(Uberblock)));
        print_uber(ub);
    }
    free(ub);
}




int main() {
    FILE *fp;

    fp = fopen("/home/nova/Projects/CPP/zfsfun/data/test", "rb");
    if (fp == NULL) {
        fprintf(stderr, "File error");
        exit(1);
    }


    //TODO: read all vdev labels for redundancy
    for (int i = 0; i < 128; i++) {
        fseek(fp, (128 + i)*1024, SEEK_SET);
        read_uber(fp);
    }


    fclose(fp);
    return 0;
}

