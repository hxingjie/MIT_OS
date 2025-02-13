#define O_RDONLY  0x000
#define O_WRONLY  0x001
#define O_RDWR    0x002
#define O_CREATE  0x200
#define O_TRUNC   0x400

//#ifdef LAB_MMAP
#define PROT_NONE       0x0 // 0000
#define PROT_READ       0x1 // 0001
#define PROT_WRITE      0x2 // 0010
#define PROT_EXEC       0x4 // 0100

#define MAP_SHARED      0x01 // 0000 0001 // should write back to the file
#define MAP_PRIVATE     0x02 // 0000 0010 // should not write back to the file
//#endif
