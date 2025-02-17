#include "kernel/types.h"
#include "kernel/param.h"
#include "user/user.h"

int main(int argc, char* argv[]) {
    // echo hello too | xargs echo bye
    // echo bye hello too

    // find . b | xargs grep hello
    // 0: ./a/b\n./c/b\n./b\n
    // grep hello ./a/b
    // grep hello ./c/b
    // grep hello ./b

    char buf[MAXARG][MAXARG];
    memset(buf, '\0', MAXARG*MAXARG);

    int arg_cnt = 0, i = 0;
    char c;
    while (read(0, &c, 1) != 0) {
        if (c == '\n') {
            arg_cnt += 1;

            char* xargs_argv[MAXARG];

            for (i = 1; i < argc; i++) {
                xargs_argv[i-1] = argv[i];
            }
            for (int j = 0; j < arg_cnt; j++, i++) {
                xargs_argv[i-1] = buf[j];
            }
            
            int pid = fork();
            if (pid == 0) {
                exec(xargs_argv[0], xargs_argv);
            }

            wait((int*)0);

            memset(buf, '\0', MAXARG*MAXARG);
            arg_cnt = 0, i = 0;
        } else if (c == ' ') {
            arg_cnt += 1;
            i = 0;
        } else {
            buf[arg_cnt][i] = c;
            i += 1;
        }
    }

    exit(0);
}