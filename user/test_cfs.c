#include "kernel/types.h"
#include "user/user.h"

void set_and_get() {
    int nice = get_nice();
    printf("user: nice: %d\n", nice);

    set_nice(6);

    nice = get_nice();
    printf("user: nice: %d\n", nice);
}

int main(int argc, char *argv[]) {
    int a = 0;

    int pid;
    pid = fork();
    if (pid == 0) { // child
        set_nice(20);

        for(int i = 0; i < 500; i++) {
            a += 1;
            //printf("child%d\n", a);
        }
        printf("parent%d\n", a);
        
        exit(0);
    } else { // parent
        set_nice(-19);

        for(int i = 0; i < 500; i++) {
            a += 1;
            //printf("parent%d\n", a);
        }
        printf("parent%d\n", a);

        wait((int*)0);
        exit(0);
    }
}