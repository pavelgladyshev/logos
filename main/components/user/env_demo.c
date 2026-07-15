/* env_demo - demonstrate the environment-variable API */
#include "libc.h"

int main(void)
{
    char *envp[17];
    int count, i;

    puts("=== Environment Variable Demo ===\n\nInherited environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) printf("  %s\n", envp[i]);
    if (count == 0) puts("  (none)\n");

    printf("\ngetenv(\"PATH\") = %s\n", getenv("PATH") ? getenv("PATH") : "(null)");
    puts("\nCalling setenv(\"GREETING\", \"hello world\")...\n");
    setenv("GREETING", "hello world");
    printf("getenv(\"GREETING\") = %s\n", getenv("GREETING") ? getenv("GREETING") : "(null)");

    puts("\nUpdated environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) printf("  %s\n", envp[i]);

    puts("\nCalling unsetenv(\"GREETING\")...\n");
    unsetenv("GREETING");
    puts("Final environment:\n");
    count = env_to_envp(envp);
    for (i = 0; i < count; i++) printf("  %s\n", envp[i]);
    return 0;
}
