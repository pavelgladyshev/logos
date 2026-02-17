/*
 * Shell - Interactive command interpreter for RISC-V kernel
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Built-in commands: help, exit, echo, set, unset
 * Supports environment variables with $VAR expansion and PATH-based lookup.
 * Environment is kernel-managed; external programs inherit it automatically.
 */

#include "libc.h"

#define MAX_CMD_LEN  128
#define MAX_ARGS     8
#define MAX_PATH_LEN 64

/*
 * Expand $VAR references in a string.
 * Writes the expanded result into 'out' (max outsize-1 chars).
 * Handles $NAME (alphanumeric + underscore) and ${NAME}.
 */
static void expand_vars(const char *in, char *out, int outsize)
{
    int o = 0;

    while (*in && o < outsize - 1) {
        if (*in == '$') {
            in++;
            /* Extract variable name */
            char name[32];
            int n = 0;
            if (*in == '?') {
                /* $? — last exit code */
                in++;
                name[0] = '?';
                n = 1;
            } else if (*in == '{') {
                /* ${NAME} form */
                in++;
                while (*in && *in != '}' && n < (int)sizeof(name) - 1) {
                    name[n++] = *in++;
                }
                if (*in == '}') in++;
            } else {
                /* $NAME form — alphanumeric and underscore */
                while ((*in >= 'A' && *in <= 'Z') ||
                       (*in >= 'a' && *in <= 'z') ||
                       (*in >= '0' && *in <= '9') ||
                       *in == '_') {
                    if (n < (int)sizeof(name) - 1) name[n++] = *in;
                    in++;
                }
            }
            name[n] = '\0';

            if (n > 0) {
                char *val = getenv(name);
                if (val) {
                    while (*val && o < outsize - 1) {
                        out[o++] = *val++;
                    }
                }
            } else {
                /* Bare '$' at end of string or before non-alnum */
                if (o < outsize - 1) out[o++] = '$';
            }
        } else {
            out[o++] = *in++;
        }
    }
    out[o] = '\0';
}

/*
 * Parse command line into argv array.
 * Modifies 'line' in place by replacing spaces with NULs.
 * Returns argc (number of arguments).
 *
 * Note: argv array is built at runtime to avoid PIE relocation issues.
 */
static int parse_cmd(char *line, char *argv[], int max_args)
{
    int argc = 0;
    int in_word = 0;

    while (*line && argc < max_args - 1) {
        if (*line == ' ' || *line == '\t') {
            if (in_word) {
                *line = '\0';  /* Terminate current argument */
                in_word = 0;
            }
        } else {
            if (!in_word) {
                argv[argc++] = line;  /* Start new argument */
                in_word = 1;
            }
        }
        line++;
    }
    argv[argc] = (char *)0;  /* NULL terminate argv array */
    return argc;
}

/*
 * Built-in command: help
 */
static void cmd_help(void)
{
    puts("RISC-V Shell - Built-in commands:\n");
    puts("  help             - Show this help message\n");
    puts("  exit [code]      - Exit shell with optional exit code\n");
    puts("  echo [args..]    - Print arguments (supports $VAR expansion)\n");
    puts("  set              - List all environment variables\n");
    puts("  set VAR=value    - Set environment variable\n");
    puts("  unset VAR        - Remove environment variable\n");
    puts("  cd [dir]         - Change current directory\n");
    puts("\n");
    puts("Variables: $VAR or ${VAR} are expanded in all arguments.\n");
    puts("PATH is used to locate commands (default: /bin).\n");
}

/*
 * Built-in command: echo
 */
static void cmd_echo(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        puts(argv[i]);
    }
    putchar('\n');
}

/*
 * Built-in command: unset VAR
 */
static void cmd_unset(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (unsetenv(argv[i]) < 0) {
            printf("unset: %s: not set\n", argv[i]);
        }
    }
}

/*
 * Built-in command: set [VAR=value ...]
 * With no args: list all variables.
 * With args: set each VAR=value pair.
 */
static void cmd_set(int argc, char *argv[])
{
    int i;

    if (argc == 1) {
        /* List all variables */
        int count = env_count();
        char buf[64];
        for (i = 0; i < count; i++) {
            getenv_entry(i, buf, sizeof(buf));
            puts(buf);
            putchar('\n');
        }
        return;
    }

    /* Set variables */
    for (i = 1; i < argc; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq == (char *)0) {
            printf("set: usage: set VAR=value\n");
            continue;
        }
        *eq = '\0';
        setenv(argv[i], eq + 1);
        *eq = '=';
    }
}

/*
 * Built-in command: cd [dir]
 * Change current working directory (default: /).
 */
static void cmd_cd(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";
    if (chdir(path) < 0) {
        printf("cd: %s: not found\n", path);
    }
}

/*
 * Try to execute an external program.
 * Searches PATH directories for the command if it doesn't contain '/'.
 * Environment is kernel-managed; child programs inherit it automatically.
 */
static void run_external(int argc, char *argv[])
{
    char path[MAX_PATH_LEN];
    int result;

    if (strchr(argv[0], '/') != (char *)0) {
        /* Path contains '/' — use as-is */
        strncpy(path, argv[0], sizeof(path) - 1);
        path[sizeof(path) - 1] = '\0';
        result = exec(path, argv);
        printf("sh: %s: not found (error %d)\n", argv[0], result);
        return;
    }

    /* Search PATH directories */
    char *path_var = getenv("PATH");
    if (path_var == (char *)0) {
        path_var = "/bin";
    }

    /* Walk colon-separated PATH entries */
    const char *p = path_var;
    while (*p) {
        /* Extract one directory from PATH */
        int dlen = 0;
        while (p[dlen] && p[dlen] != ':') dlen++;

        /* Build "dir/cmd" path */
        if (dlen > 0 && dlen < (int)sizeof(path) - 2) {
            memcpy(path, p, dlen);
            path[dlen] = '/';
            strncpy(path + dlen + 1, argv[0], sizeof(path) - dlen - 2);
            path[sizeof(path) - 1] = '\0';

            result = exec(path, argv);
            /* If exec returns, it failed — try next PATH entry */
        }

        /* Advance past this entry */
        p += dlen;
        if (*p == ':') p++;
    }

    printf("sh: %s: command not found\n", argv[0]);
}

int main(int argc, char *argv[])
{
    char line[MAX_CMD_LEN];
    char expanded[MAX_CMD_LEN];
    char *cmd_argv[MAX_ARGS];
    int cmd_argc;

    /* Set default environment if PATH is not already set */
    if (getenv("PATH") == (char *)0) {
        setenv("PATH", "/bin");
    }

    for (;;) {
        char *cwd = getenv("CWD");
        if (cwd) puts(cwd);
        puts("$ ");
        gets(line, MAX_CMD_LEN);

        /* Skip empty lines */
        if (line[0] == '\0') continue;

        /* Expand $VAR references */
        expand_vars(line, expanded, MAX_CMD_LEN);

        /* Parse command line */
        cmd_argc = parse_cmd(expanded, cmd_argv, MAX_ARGS);
        if (cmd_argc == 0) continue;

        /* Check built-in commands */
        if (strcmp(cmd_argv[0], "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd_argv[0], "exit") == 0) {
            int code = 0;
            if (cmd_argc > 1) {
                /* Parse exit code - simple atoi */
                char *p = cmd_argv[1];
                while (*p >= '0' && *p <= '9') {
                    code = code * 10 + (*p - '0');
                    p++;
                }
            }
            exit(code);
        } else if (strcmp(cmd_argv[0], "echo") == 0) {
            cmd_echo(cmd_argc, cmd_argv);
        } else if (strcmp(cmd_argv[0], "set") == 0) {
            cmd_set(cmd_argc, cmd_argv);
        } else if (strcmp(cmd_argv[0], "unset") == 0) {
            cmd_unset(cmd_argc, cmd_argv);
        } else if (strcmp(cmd_argv[0], "cd") == 0) {
            cmd_cd(cmd_argc, cmd_argv);
        } else {
            /* Try external program */
            run_external(cmd_argc, cmd_argv);
        }
    }

    return 0;
}
