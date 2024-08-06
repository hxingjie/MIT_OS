# Lab 1

## config
```
cd Projects/
cd xv6-labs-2020/
git checkout util
make qemu
```

## test copy.c
```
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ sudo apt-install vim
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ cd user
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ vim copy.c
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
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ vim Makefile
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
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ make qemu
$ copy // 调用
abc
abc
```

## sleep
### 写代码
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020/user/ vim sleep.c
```c
// sleep.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("must 1 argument for sleep\n");
        exit(1); // 执行失败，退出
    }
    
    int sleepTrick = atoi(argv[1]); // 字符串转换为int
    printf("(nothing happens for a little while)\n");
    sleep(sleepTrick);

    exit(0);
}
```

### 更新Makefile
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ vim Makefile
```
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ vim Makefile
UPROGS=\
        $U/_cat\
        $U/_echo\
        ...
        $U/_zombie\
        $U/_sleep\ // 添加到此处
```

### 验证
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ make qemu
$ sleep 10

sudo ln -s /usr/bin/python3 /usr/bin/python // 为python3创建软连接
/usr/bin: ls -l 可以看到创建的软连接
ubuntu-os@ubuntu:~/Projects/xv6-labs-2020$ ./grade-lab-util sleep
result:
    make: 'kernel/kernel' is up to date.
    == Test sleep, no arguments == sleep, no arguments: OK (1.1s) 
    == Test sleep, returns == sleep, returns: OK (0.9s) 
    == Test sleep, makes syscall == sleep, makes syscall: OK (1.1s)


