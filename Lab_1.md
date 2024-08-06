# Lab 1

## config
```
cd Projects/
cd xv6-labs-2020/
git checkout util
make qemu
```

##
```
sudo apt-install vim
xv6-labs-2020/: cd user
vim copy.c
```

```c
#include "kernel/types.h"
#include "user/user.h"

int main() {
    char buf[64];

    while (1) {
        int n = read(0, buf, sizeof(buf)); // sizeof(buf)能够获取数组大小
        
        if (n <= 0) break;
        
        write(1, buf, n);
    }
    
    exit(0);
}
```

```
xv6-labs-2020/ vim Makefile
UPROGS=\
        $U/_cat\
        $U/_echo\
        $U/_forktest\
        $U/_grep\
        $U/_init\
        $U/_kill\
        $U/_ln\
        $U/_ls\
        $U/_mkdir\
        $U/_rm\
        $U/_sh\
        $U/_stressfs\
        $U/_usertests\
        $U/_grind\
        $U/_wc\
        $U/_zombie\
        $U/_copy\ // 添加到此处
```

```
make qemu
copy // 调用
```
