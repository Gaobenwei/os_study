// On-disk file system format.
// Both the kernel and user programs use this header file.


#define ROOTINO  1   // root i-number
#define BSIZE 1024  // block size

// Disk layout:
// [ boot block | super block | log | inode blocks |
//                                          free bit map | data blocks]
//
// mkfs computes the super block and builds an initial file system. The
// super block describes the disk layout:
struct superblock {
  uint magic;        // Must be FSMAGIC
  uint size;         // Size of file system image (blocks)
  uint nblocks;      // Number of data blocks
  uint ninodes;      // Number of inodes.
  uint nlog;         // Number of log blocks
  uint logstart;     // Block number of first log block
  uint inodestart;   // Block number of first inode block
  uint bmapstart;    // Block number of first free map block
};

#define FSMAGIC 0x10203040

#define NDIRECT 11
#define NINDIRECT (BSIZE / sizeof(uint))
#define NDINDIRECT (NINDIRECT*NINDIRECT)
#define MAXFILE (NDIRECT + NINDIRECT+NDINDIRECT)

// On-disk inode structure
struct dinode {
  short type;           // File type
  short major;          // Major device number (T_DEVICE only) 主要设备号(仅限T_DEVICE)
  short minor;          // Minor device number (T_DEVICE only) 次要设备号(仅限T_DEVICE)
  short nlink;          // Number of links to inode in file system 文件系统中inode的链接数
  uint size;            // Size of file (bytes)
  uint addrs[NDIRECT+2];   // Data block addresses
};

// Inodes per block. 每个块的节点数。
#define IPB           (BSIZE / sizeof(struct dinode))

// Block containing inode i 包含索引节点i的块
#define IBLOCK(i, sb)     ((i) / IPB + sb.inodestart)

// Bitmap bits per block 每个块的位图位
#define BPB           (BSIZE*8)

// Block of free map containing bit for block b
// 包含块b位的空闲映射块
#define BBLOCK(b, sb) ((b)/BPB + sb.bmapstart)

// Directory is a file containing a sequence of dirent structures.
// 目录是一个包含一系列不同结构的文件。
#define DIRSIZ 14

struct dirent {
  ushort inum;
  char name[DIRSIZ];
};

