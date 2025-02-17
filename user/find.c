#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

#include "user/user.h"

// Write a simple version of the UNIX find program: 
// find all the files in a directory tree with a specific name. 

/*
Some hints:
    Look at user/ls.c to see how to read directories.
    Use recursion to allow find to descend into sub-directories.
    Don't recurse into "." and "..".
    Changes to the file system persist across runs of qemu; to get a clean file system run make clean and then make qemu.
    You'll need to use C strings. Have a look at K&R (the C book), for example Section 5.5.
    Note that == does not compare strings like in Python. Use strcmp() instead.
    Add the program to UPROGS in Makefile.
*/
int find(char *path, char* filename);

int main(int argc, char* argv[]) {
    if(argc != 3){
        fprintf(2, "input: find {path} {filename}");
        exit(1);
    }

    int res = find(argv[1], argv[2]);

    exit(res);
}

int find(char* path, char* filename) {
    // check
    int fd;
    if((fd = open(path, 0)) < 0){
        fprintf(2, "find: cannot open %s\n", path); // 2 is debug console
        return 1;
    }

    struct stat st;
    if(fstat(fd, &st) < 0){
        fprintf(2, "ls: cannot stat %s\n", path); // 2 is debug console
        close(fd);
        return 1;
    }

    if (st.type == T_FILE) {
        fprintf(2, "path isn't a dir!\n");
        return 1;
    }

    char buf[512];
    char* p;
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf){
        fprintf(2, "ls: path too long\n");
        return 1;
    }
    strcpy(buf, path);
    p = buf + strlen(buf);
    *p++ = '/'; // 在buf后面追加'\'，再往前移一个下标

    struct dirent de;
    while(read(fd, &de, sizeof(de)) == sizeof(de)){
        if(de.inum == 0)
            continue;
        
        memmove(p, de.name, DIRSIZ);
        p[DIRSIZ] = 0;
        if(stat(buf, &st) < 0){ // read file's intro
            printf("ls: cannot stat %s\n", buf);
            continue;
        }

        // buf: path/name
        if (st.type == T_DIR && strcmp(de.name, ".") != 0 && strcmp(de.name, "..") != 0) {
            char path_n[512];
            strcpy(path_n, buf);
            find(path_n, filename);
        } else if (st.type == T_FILE && strcmp(de.name, filename) == 0) {
            printf("%s\n", buf);
        }
    }

    close(fd);

    return 0;
}