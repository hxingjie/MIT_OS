//
// File-system system calls.
// Mostly argument checking, since we don't trust
// user code, and calls into file.c and fs.c.
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "fs.h"
#include "sleeplock.h"
#include "file.h"
#include "fcntl.h"

// Fetch the nth word-sized system call argument as a file descriptor
// and return both the descriptor and the corresponding struct file.
static int
argfd(int n, int *pfd, struct file **pf)
{
  int fd;
  struct file *f;

  if(argint(n, &fd) < 0)
    return -1;
  if(fd < 0 || fd >= NOFILE || (f=myproc()->ofile[fd]) == 0)
    return -1;
  if(pfd)
    *pfd = fd;
  if(pf)
    *pf = f;
  return 0;
}

// Allocate a file descriptor for the given file.
// Takes over file reference from caller on success.
static int
fdalloc(struct file *f)
{
  int fd;
  struct proc *p = myproc();

  for(fd = 0; fd < NOFILE; fd++){
    if(p->ofile[fd] == 0){
      p->ofile[fd] = f;
      return fd;
    }
  }
  return -1;
}

uint64
sys_dup(void)
{
  struct file *f;
  int fd;

  if(argfd(0, 0, &f) < 0)
    return -1;
  if((fd=fdalloc(f)) < 0)
    return -1;
  filedup(f);
  return fd;
}

uint64
sys_read(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;
  return fileread(f, p, n);
}

uint64
sys_write(void)
{
  struct file *f;
  int n;
  uint64 p;

  if(argfd(0, 0, &f) < 0 || argint(2, &n) < 0 || argaddr(1, &p) < 0)
    return -1;

  return filewrite(f, p, n);
}

uint64
sys_close(void)
{
  int fd;
  struct file *f;

  if(argfd(0, &fd, &f) < 0)
    return -1;
  myproc()->ofile[fd] = 0;
  fileclose(f);
  return 0;
}

uint64
sys_fstat(void)
{
  struct file *f;
  uint64 st; // user pointer to struct stat

  if(argfd(0, 0, &f) < 0 || argaddr(1, &st) < 0)
    return -1;
  return filestat(f, st);
}

// Create the path new as a link to the same inode as old.
uint64
sys_link(void)
{
  char name[DIRSIZ], new[MAXPATH], old[MAXPATH];
  struct inode *dp, *ip;

  if(argstr(0, old, MAXPATH) < 0 || argstr(1, new, MAXPATH) < 0)
    return -1;

  begin_op();
  if((ip = namei(old)) == 0){
    end_op();
    return -1;
  }

  ilock(ip);
  if(ip->type == T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }

  ip->nlink++;
  iupdate(ip);
  iunlock(ip);

  if((dp = nameiparent(new, name)) == 0)
    goto bad;
  ilock(dp);
  if(dp->dev != ip->dev || dirlink(dp, name, ip->inum) < 0){
    iunlockput(dp);
    goto bad;
  }
  iunlockput(dp);
  iput(ip);

  end_op();

  return 0;

bad:
  ilock(ip);
  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);
  end_op();
  return -1;
}

// Is the directory dp empty except for "." and ".." ?
static int
isdirempty(struct inode *dp)
{
  int off;
  struct dirent de;

  for(off=2*sizeof(de); off<dp->size; off+=sizeof(de)){
    if(readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("isdirempty: readi");
    if(de.inum != 0)
      return 0;
  }
  return 1;
}

uint64
sys_unlink(void)
{
  struct inode *ip, *dp;
  struct dirent de;
  char name[DIRSIZ], path[MAXPATH];
  uint off;

  if(argstr(0, path, MAXPATH) < 0)
    return -1;

  begin_op();
  if((dp = nameiparent(path, name)) == 0){
    end_op();
    return -1;
  }

  ilock(dp);

  // Cannot unlink "." or "..".
  if(namecmp(name, ".") == 0 || namecmp(name, "..") == 0)
    goto bad;

  if((ip = dirlookup(dp, name, &off)) == 0)
    goto bad;
  ilock(ip);

  if(ip->nlink < 1)
    panic("unlink: nlink < 1");
  if(ip->type == T_DIR && !isdirempty(ip)){
    iunlockput(ip);
    goto bad;
  }

  memset(&de, 0, sizeof(de));
  if(writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    panic("unlink: writei");
  if(ip->type == T_DIR){
    dp->nlink--;
    iupdate(dp);
  }
  iunlockput(dp);

  ip->nlink--;
  iupdate(ip);
  iunlockput(ip);

  end_op();

  return 0;

bad:
  iunlockput(dp);
  end_op();
  return -1;
}

static struct inode*
create(char *path, short type, short major, short minor)
{
  struct inode *ip, *dp;
  char name[DIRSIZ];

  if((dp = nameiparent(path, name)) == 0)
    return 0;

  ilock(dp);

  if((ip = dirlookup(dp, name, 0)) != 0){
    iunlockput(dp);
    ilock(ip);
    if(type == T_FILE && (ip->type == T_FILE || ip->type == T_DEVICE))
      return ip;
    iunlockput(ip);
    return 0;
  }

  if((ip = ialloc(dp->dev, type)) == 0)
    panic("create: ialloc");

  ilock(ip);
  ip->major = major;
  ip->minor = minor;
  ip->nlink = 1;
  iupdate(ip);

  if(type == T_DIR){  // Create . and .. entries.
    dp->nlink++;  // for ".."
    iupdate(dp);
    // No ip->nlink++ for ".": avoid cyclic ref count.
    if(dirlink(ip, ".", ip->inum) < 0 || dirlink(ip, "..", dp->inum) < 0)
      panic("create dots");
  }

  if(dirlink(dp, name, ip->inum) < 0)
    panic("create: dirlink");

  iunlockput(dp);

  return ip;
}

uint64
sys_open(void)
{
  char path[MAXPATH];
  int fd, omode;
  struct file *f;
  struct inode *ip;
  int n;

  if((n = argstr(0, path, MAXPATH)) < 0 || argint(1, &omode) < 0)
    return -1;

  begin_op();

  if(omode & O_CREATE){
    ip = create(path, T_FILE, 0, 0);
    if(ip == 0){
      end_op();
      return -1;
    }
  } else {
    if((ip = namei(path)) == 0){
      end_op();
      return -1;
    }
    ilock(ip);
    if(ip->type == T_DIR && omode != O_RDONLY){
      iunlockput(ip);
      end_op();
      return -1;
    }
  }

  if(ip->type == T_DEVICE && (ip->major < 0 || ip->major >= NDEV)){
    iunlockput(ip);
    end_op();
    return -1;
  }

  if((f = filealloc()) == 0 || (fd = fdalloc(f)) < 0){
    if(f)
      fileclose(f);
    iunlockput(ip);
    end_op();
    return -1;
  }

  if(ip->type == T_DEVICE){
    f->type = FD_DEVICE;
    f->major = ip->major;
  } else {
    f->type = FD_INODE;
    f->off = 0;
  }
  f->ip = ip;
  f->readable = !(omode & O_WRONLY);
  f->writable = (omode & O_WRONLY) || (omode & O_RDWR);

  if((omode & O_TRUNC) && ip->type == T_FILE){
    itrunc(ip);
  }

  iunlock(ip);
  end_op();

  return fd;
}

uint64
sys_mkdir(void)
{
  char path[MAXPATH];
  struct inode *ip;

  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = create(path, T_DIR, 0, 0)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_mknod(void)
{
  struct inode *ip;
  char path[MAXPATH];
  int major, minor;

  begin_op();
  if((argstr(0, path, MAXPATH)) < 0 ||
     argint(1, &major) < 0 ||
     argint(2, &minor) < 0 ||
     (ip = create(path, T_DEVICE, major, minor)) == 0){
    end_op();
    return -1;
  }
  iunlockput(ip);
  end_op();
  return 0;
}

uint64
sys_chdir(void)
{
  char path[MAXPATH];
  struct inode *ip;
  struct proc *p = myproc();
  
  begin_op();
  if(argstr(0, path, MAXPATH) < 0 || (ip = namei(path)) == 0){
    end_op();
    return -1;
  }
  ilock(ip);
  if(ip->type != T_DIR){
    iunlockput(ip);
    end_op();
    return -1;
  }
  iunlock(ip);
  iput(p->cwd);
  end_op();
  p->cwd = ip;
  return 0;
}

uint64
sys_exec(void)
{
  char path[MAXPATH], *argv[MAXARG];
  int i;
  uint64 uargv, uarg;

  if(argstr(0, path, MAXPATH) < 0 || argaddr(1, &uargv) < 0){
    return -1;
  }
  memset(argv, 0, sizeof(argv));
  for(i=0;; i++){
    if(i >= NELEM(argv)){
      goto bad;
    }
    if(fetchaddr(uargv+sizeof(uint64)*i, (uint64*)&uarg) < 0){
      goto bad;
    }
    if(uarg == 0){
      argv[i] = 0;
      break;
    }
    argv[i] = kalloc();
    if(argv[i] == 0)
      goto bad;
    if(fetchstr(uarg, argv[i], PGSIZE) < 0)
      goto bad;
  }

  int ret = exec(path, argv);

  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);

  return ret;

 bad:
  for(i = 0; i < NELEM(argv) && argv[i] != 0; i++)
    kfree(argv[i]);
  return -1;
}

uint64
sys_pipe(void)
{
  uint64 fdarray; // user pointer to array of two integers
  struct file *rf, *wf;
  int fd0, fd1;
  struct proc *p = myproc();

  if(argaddr(0, &fdarray) < 0)
    return -1;
  if(pipealloc(&rf, &wf) < 0)
    return -1;
  fd0 = -1;
  if((fd0 = fdalloc(rf)) < 0 || (fd1 = fdalloc(wf)) < 0){
    if(fd0 >= 0)
      p->ofile[fd0] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  if(copyout(p->pagetable, fdarray, (char*)&fd0, sizeof(fd0)) < 0 ||
     copyout(p->pagetable, fdarray+sizeof(fd0), (char *)&fd1, sizeof(fd1)) < 0){
    p->ofile[fd0] = 0;
    p->ofile[fd1] = 0;
    fileclose(rf);
    fileclose(wf);
    return -1;
  }
  return 0;
}

uint64 sys_mmap(void) {
    // void *mmap(void *addr, int length, int prot, int flags,
    //           int fd, int offset);
    uint64 addr;
    int length, prot, flags, fd, offset;
    if (argaddr(0, &addr) < 0 || argint(1, &length) < 0
        || argint(2, &prot) < 0 || argint(3, &flags) < 0
        || argint(4, &fd) < 0 || argint(5, &offset) < 0) {
        return -1;
    }
     
    struct proc* p = myproc();

    // mmap doesn't allow read/write mapping of a file opened read-only.
    struct file* mmapfile = p->ofile[fd];
    if (flags == MAP_SHARED && (prot & PROT_WRITE) && mmapfile->writable == 0) {
        return -1;
    }

    // lazy alloc
    int page_cnt = length % PGSIZE == 0 ? length / PGSIZE : length / PGSIZE + 1;
    uint64 unused_pages[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int idx = 0;
    
    // find unused region
    for (uint64 va = PGROUNDUP(p->sz); va < MAXVA && idx < page_cnt; va += PGSIZE) {
        pte_t* pte = walk(p->pagetable, va, 1);
        if (pte == 0) { // kalloc fail
            for (int i = 0; i < idx; i++) { // free writed pte
                pte_t* tmp = walk(p->pagetable, unused_pages[i], 0);
                *tmp = 0;
            }
            printf("kalloc fail\n");
            return -1;
        }

        if (*pte & PTE_V) {
            // unused_pages must continue
            for (int i = 0; i < idx; i++) { // free writed pte
                pte_t* tmp = walk(p->pagetable, unused_pages[i], 0);
                *tmp = 0;
            }
            idx = 0;
            continue;
        } else { // find unused page
            unused_pages[idx] = va;
            idx += 1;
            *pte |= 1 << 8; // set rsw to 01
            *pte |= (prot & (PROT_EXEC | PROT_WRITE | PROT_READ)) << 1; // set XWR
            *pte |= PTE_U;
            //printf("pte: %x\n", *pte);
        }

    }
    if (idx < page_cnt) { // find unused page fail
        printf("no enough free page\n");
        return -1;
    }

    filedup(p->ofile[fd]);

    int idx_regions;
    for (idx_regions = 0; idx_regions < 16; idx_regions++) {
        if (p->mmap_regions[idx_regions].used == 0) {
            p->mmap_regions[idx_regions].used = 1;
            p->mmap_regions[idx_regions].va = unused_pages[0];
            p->mmap_regions[idx_regions].length = length;
            p->mmap_regions[idx_regions].cur_length = length;
            p->mmap_regions[idx_regions].permission = prot;
            p->mmap_regions[idx_regions].map_type = flags;
            p->mmap_regions[idx_regions].offset = offset;
            p->mmap_regions[idx_regions].mmap_file = p->ofile[fd];
            break;
        }
    }
    if (idx_regions == 16) { // no enough free mmap_regions
        for (int i = 0; i < idx; i++) { // free writed pte
            pte_t* tmp = walk(p->pagetable, unused_pages[i], 0);
            *tmp = 0;
        }
        return -1;
    }

    p->sz += length;

    return unused_pages[0];
}

void va2regions(uint64 va, int* i_regions, int* j_regions) {
    // va should align with page
    struct proc* p = myproc();
    int i = 0, j = 0;
    for (i = 0; i < 16; i++) {
        if (p->mmap_regions[i].used == 1) {
            // init page_cnt
            int page_cnt;
            if (p->mmap_regions[i].length % PGSIZE == 0) {
                page_cnt = p->mmap_regions[i].length / PGSIZE;
            } else {
                page_cnt = p->mmap_regions[i].length / PGSIZE + 1;
            }

            // find the region
            for (j = 0; j < page_cnt; j++) {
                if (va == p->mmap_regions[i].va + j*PGSIZE) {
                    break;
                }
            }

            if (j < page_cnt) {
                break;
            }
        }
    }
    
    if (i == 16) {
        panic("no this mapped file");
    }

    *i_regions = i;
    *j_regions = j;
}

void read_from_mmapfile(uint64 va) {
    // read file
    va = PGROUNDDOWN(va);
    
    int i_regions, j_regions;
    va2regions(va, &i_regions, &j_regions);

    struct inode* ip = myproc()->mmap_regions[i_regions].mmap_file->ip;
    ilock(ip);
    readi(ip, 1, va, j_regions*PGSIZE, PGSIZE);
    iunlock(ip);
}

uint64 sys_munmap(void) {
    // int munmap(addr, length);
    uint64 beg_va;
    int length;
    if (argaddr(0, &beg_va) < 0 || argint(1, &length) < 0) {
        return -1;
    }
    
    // 1.find the VMA for the address range and unmap the specified pages (hint: use uvmunmap).
    if (beg_va != PGROUNDDOWN(beg_va)) {
        panic("sys_munmap, not align");
    }

    struct proc* p = myproc();
    int unmap_page_cnt = length % PGSIZE == 0 ? length / PGSIZE : length / PGSIZE + 1;
    for (int i = 0; i < unmap_page_cnt; i++) {
        uint64 va = beg_va + i * PGSIZE;
        int i_regions, j_regions;
        va2regions(va, &i_regions, &j_regions);

        pte_t* pte = walk(p->pagetable, va, 0);

        // write back to file
        if (*pte & PTE_V && p->mmap_regions[i_regions].map_type == MAP_SHARED) {
            filewrite(p->mmap_regions[i_regions].mmap_file, va, PGSIZE);
        }

        // update pagetable
        uvmunmap(p->pagetable, va, 1, 1); // unmap in pagetable and free data page
        p->sz -= PGSIZE;

        // update p->mmap_regions
        if (p->mmap_regions[i_regions].cur_length > PGSIZE) {
            // va not need to update
            p->mmap_regions[i_regions].cur_length -= PGSIZE;
        } else {
            // subtract file's ref
            fileclose(p->mmap_regions[i_regions].mmap_file);
            // free this mmap_region
            p->mmap_regions[i_regions].used = 0;
        }
        
    }

    return 0;
}
