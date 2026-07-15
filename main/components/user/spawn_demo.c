/* spawn_demo - launch /bin/hello through spawn() */
#include "libc.h"

int main(void)
{
    char *hello_argv[5];
    int result;

    hello_argv[0] = "hello";
    hello_argv[1] = "arg1";
    hello_argv[2] = "arg2";
    hello_argv[3] = "test argument 3";
    hello_argv[4] = (char *)0;

    result = spawn("/bin/hello", hello_argv);
    if (result < 0) {
        printf("spawn failed with code %d\n", result);
        return 1;
    }
    printf("spawn_demo: child exited with code %d\n", result);
    return 0;
}
