/*
 * Shell - Interactive command interpreter for RISC-V kernel
 * Licensed under Creative Commons Attribution International License 4.0
 *
 * Built-in commands: help, exit, echo, set, unset, cd
 * Supports environment variables with $VAR expansion and PATH-based lookup.
 * External commands run via fork()+exec()+wait() (Unix-style).
 * Each child process inherits a copy of the shell's environment via fork().
 *
 * I/O redirection: cmd > file, cmd >> file, cmd < file
 * Pipes: cmd1 | cmd2 [| cmd3 ...] (up to MAX_PIPE_STAGES stages)
 */

#include "libc.h"

#define MAX_CMD_LEN      128
#define MAX_ARGS         8
#define MAX_PATH_LEN     64
#define MAX_PIPE_STAGES  4

/* Redirection modes */
#define REDIR_NONE    0
#define REDIR_TRUNC   1   /* >  */
#define REDIR_APPEND  2   /* >> */

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
 * Parse redirection operators from argv.
 * Scans for >, >>, < tokens and extracts filenames.
 * Removes redirection tokens from argv, adjusting argc.
 *
 * redir_out:      receives output filename (or empty string)
 * redir_out_mode: receives REDIR_NONE, REDIR_TRUNC, or REDIR_APPEND
 * redir_in:       receives input filename (or empty string)
 * has_redir_in:   set to 1 if < found, 0 otherwise
 *
 * Returns new argc after removing redirection tokens.
 */
static int parse_redirections(int argc, char *argv[],
                              char *redir_out, int *redir_out_mode,
                              char *redir_in, int *has_redir_in)
{
    int i;
    int new_argc = 0;

    redir_out[0] = '\0';
    redir_in[0] = '\0';
    *redir_out_mode = REDIR_NONE;
    *has_redir_in = 0;

    i = 0;
    while (i < argc) {
        /* Check for >> (must check before >) */
        if (argv[i][0] == '>' && argv[i][1] == '>' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_out, argv[i + 1], MAX_PATH_LEN - 1);
                redir_out[MAX_PATH_LEN - 1] = '\0';
                *redir_out_mode = REDIR_APPEND;
                i += 2;
                continue;
            }
        }
        /* Check for > */
        else if (argv[i][0] == '>' && argv[i][1] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_out, argv[i + 1], MAX_PATH_LEN - 1);
                redir_out[MAX_PATH_LEN - 1] = '\0';
                *redir_out_mode = REDIR_TRUNC;
                i += 2;
                continue;
            }
        }
        /* Check for < */
        else if (argv[i][0] == '<' && argv[i][1] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_in, argv[i + 1], MAX_PATH_LEN - 1);
                redir_in[MAX_PATH_LEN - 1] = '\0';
                *has_redir_in = 1;
                i += 2;
                continue;
            }
        }

        /* Keep this token */
        argv[new_argc++] = argv[i];
        i++;
    }
    argv[new_argc] = (char *)0;
    return new_argc;
}

/*
 * Apply redirections in the current process.
 * Opens files and uses dup2() to redirect stdin/stdout.
 * Returns 0 on success, -1 on error.
 */
static int apply_redirections(const char *redir_out, int redir_out_mode,
                              const char *redir_in, int has_redir_in)
{
    int fd;

    /* Input redirection: < file */
    if (has_redir_in) {
        fd = open(redir_in, O_RDONLY);
        if (fd < 0) {
            printf("sh: %s: No such file\n", redir_in);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    /* Output redirection: > file (truncate) */
    if (redir_out_mode == REDIR_TRUNC) {
        fd = open(redir_out, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) {
            printf("sh: %s: Cannot open for writing\n", redir_out);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
    /* Output redirection: >> file (append) */
    else if (redir_out_mode == REDIR_APPEND) {
        fd = open(redir_out, O_WRONLY | O_CREAT | O_APPEND);
        if (fd < 0) {
            printf("sh: %s: Cannot open for appending\n", redir_out);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }

    return 0;
}

/*
 * Built-in command: help
 */
static void cmd_help(void)
{
    puts("RISC-V Shell - Built-in commands:");
    puts("  help             - Show this help message");
    puts("  exit [code]      - Exit shell with optional exit code");
    puts("  echo [args..]    - Print arguments (supports $VAR expansion)");
    puts("  set              - List all environment variables");
    puts("  set VAR=value    - Set environment variable");
    puts("  unset VAR        - Remove environment variable");
    puts("  cd [dir]         - Change current directory");
    puts("");
    puts("Redirection: cmd > file, cmd >> file, cmd < file");
    puts("Pipes:       cmd1 | cmd2 [| cmd3 ...]  (up to 4 stages)");
    puts("Variables:   $VAR or ${VAR} are expanded in all arguments.");
    puts("PATH is used to locate commands (default: /bin).");
}

/*
 * Built-in command: echo
 * Writes to stdout (which may have been redirected via dup2).
 */
static void cmd_echo(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
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
 * Try to exec a command with PATH lookup.
 * Tries argv[0] directly if it contains '/', otherwise searches PATH.
 * Only returns if exec fails.
 */
static void exec_with_path(char *argv[])
{
    char path[MAX_PATH_LEN];

    if (strchr(argv[0], '/') != (char *)0) {
        /* Path contains '/' — use as-is */
        exec(argv[0], argv);
        /* exec() only returns on failure */
        printf("sh: %s: not found\n", argv[0]);
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

            exec(path, argv);
            /* exec() only returns on failure — try next PATH entry */
        }

        /* Advance past this entry */
        p += dlen;
        if (*p == ':') p++;
    }

    printf("sh: %s: command not found\n", argv[0]);
}

/*
 * Run an external program using fork()+exec()+wait().
 *
 * The shell forks a child process, which sets up any redirections,
 * then execs the command.
 *
 * The parent waits for the child to finish. The kernel sets $? in the
 * parent's environment when the child exits.
 */
static void run_external(int argc, char *argv[],
                         const char *redir_out, int redir_out_mode,
                         const char *redir_in, int has_redir_in)
{
    int pid;

    (void)argc;

    pid = fork();
    if (pid < 0) {
        printf("sh: fork failed\n");
        return;
    }

    if (pid == 0) {
        /* ---- Child process ---- */

        /* Apply I/O redirections */
        if (apply_redirections(redir_out, redir_out_mode,
                               redir_in, has_redir_in) < 0) {
            exit(1);
        }

        /* Exec the command */
        exec_with_path(argv);
        exit(127);
    }

    /* ---- Parent process ---- */
    wait();
    /* $? is set by the kernel when the child exits */
}

/*
 * Run a multi-stage pipeline: cmd1 | cmd2 | ... | cmdN
 *
 * Creates N-1 pipes, forks N children, wires stdin/stdout between stages.
 * Each stage can have its own < or > redirections.
 * Parent waits for all children.
 */
static void run_multi_pipeline(int n_stages, char **argv_s[],
                                char rout_s[][MAX_PATH_LEN],
                                int rout_mode_s[],
                                char rin_s[][MAX_PATH_LEN],
                                int has_rin_s[])
{
    int pfd[2];
    int prev_read_fd = -1;
    int pid;
    int i, forked;

    forked = 0;
    for (i = 0; i < n_stages; i++) {
        /* Create pipe between this stage and the next (not for last stage) */
        if (i < n_stages - 1) {
            if (pipe(pfd) < 0) {
                printf("sh: pipe failed\n");
                break;
            }
        }

        pid = fork();
        if (pid < 0) {
            printf("sh: fork failed\n");
            if (i < n_stages - 1) {
                close(pfd[0]);
                close(pfd[1]);
            }
            break;
        }

        if (pid == 0) {
            /* ---- Child process ---- */

            /* Wire stdin from previous pipe's read end */
            if (prev_read_fd >= 0) {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }

            /* Wire stdout to current pipe's write end (not last stage) */
            if (i < n_stages - 1) {
                close(pfd[0]);  /* Close read end (parent keeps it) */
                dup2(pfd[1], STDOUT_FILENO);
                close(pfd[1]);
            }

            /* Apply per-stage redirections (may override pipe wiring) */
            if (apply_redirections(rout_s[i], rout_mode_s[i],
                                   rin_s[i], has_rin_s[i]) < 0) {
                exit(1);
            }

            exec_with_path(argv_s[i]);
            exit(127);
        }

        /* ---- Parent process ---- */
        forked++;

        /* Close previous read end (child inherited it) */
        if (prev_read_fd >= 0)
            close(prev_read_fd);

        /* Keep read end for next stage, close write end */
        if (i < n_stages - 1) {
            close(pfd[1]);
            prev_read_fd = pfd[0];
        }
    }

    /* Close any remaining pipe fd */
    if (prev_read_fd >= 0)
        close(prev_read_fd);

    /* Wait for all forked children */
    for (i = 0; i < forked; i++)
        wait();
}


int main(int argc, char *argv[])
{
    char line[MAX_CMD_LEN];
    char expanded[MAX_CMD_LEN];
    char *cmd_argv[MAX_ARGS];
    int cmd_argc;

    /* Redirection state */
    char redir_out[MAX_PATH_LEN];
    char redir_in[MAX_PATH_LEN];
    int redir_out_mode;
    int has_redir_in;

    /* Set default environment if PATH is not already set */
    if (getenv("PATH") == (char *)0) {
        setenv("PATH", "/bin");
    }

    for (;;) {
        char *cwd = getenv("CWD");
        if (cwd) write(STDOUT_FILENO, cwd, strlen(cwd));
        write(STDOUT_FILENO, "$ ", 2);
        gets(line, MAX_CMD_LEN);

        /* Skip empty lines */
        if (line[0] == '\0') continue;

        /* Expand $VAR references */
        expand_vars(line, expanded, MAX_CMD_LEN);

        /* Check for pipe operator BEFORE tokenizing (spaces matter) */
        if (strchr(expanded, '|') != (char *)0) {
            /* Split expanded into pipe-separated segments */
            char seg_bufs[MAX_PIPE_STAGES][MAX_CMD_LEN];
            char *seg_argv[MAX_PIPE_STAGES][MAX_ARGS];
            int seg_argc[MAX_PIPE_STAGES];
            char seg_rout[MAX_PIPE_STAGES][MAX_PATH_LEN];
            char seg_rin[MAX_PIPE_STAGES][MAX_PATH_LEN];
            int seg_rout_mode[MAX_PIPE_STAGES];
            int seg_has_rin[MAX_PIPE_STAGES];
            int n_stages = 0;
            int ok = 1;

            /* Walk expanded string, splitting on '|' */
            {
                char *p = expanded;
                char *seg_start = expanded;
                while (1) {
                    if (*p == '|' || *p == '\0') {
                        int seg_len = p - seg_start;
                        if (n_stages >= MAX_PIPE_STAGES) {
                            printf("sh: too many pipe stages (max %d)\n",
                                   MAX_PIPE_STAGES);
                            ok = 0;
                            break;
                        }
                        memcpy(seg_bufs[n_stages], seg_start, seg_len);
                        seg_bufs[n_stages][seg_len] = '\0';
                        n_stages++;
                        if (*p == '\0') break;
                        seg_start = p + 1;
                    }
                    p++;
                }
            }

            if (!ok || n_stages < 2) {
                if (ok) printf("sh: empty pipeline\n");
                continue;
            }

            /* Parse each segment */
            {
                int s;
                for (s = 0; s < n_stages; s++) {
                    seg_argc[s] = parse_cmd(seg_bufs[s], seg_argv[s], MAX_ARGS);
                    if (seg_argc[s] == 0) { ok = 0; break; }
                    seg_argc[s] = parse_redirections(seg_argc[s], seg_argv[s],
                                                     seg_rout[s], &seg_rout_mode[s],
                                                     seg_rin[s], &seg_has_rin[s]);
                    if (seg_argc[s] == 0) { ok = 0; break; }
                }
            }
            if (!ok) continue;

            /* Build argv pointer array for run_multi_pipeline */
            {
                int s;
                char **argv_arr[MAX_PIPE_STAGES];
                for (s = 0; s < n_stages; s++)
                    argv_arr[s] = seg_argv[s];
                run_multi_pipeline(n_stages, argv_arr,
                                   seg_rout, seg_rout_mode,
                                   seg_rin, seg_has_rin);
            }
            continue;
        }

        /* No pipe — parse as single command */
        cmd_argc = parse_cmd(expanded, cmd_argv, MAX_ARGS);
        if (cmd_argc == 0) continue;

        /* Extract redirections from argv */
        cmd_argc = parse_redirections(cmd_argc, cmd_argv,
                                      redir_out, &redir_out_mode,
                                      redir_in, &has_redir_in);
        if (cmd_argc == 0) continue;

        /* Check built-in commands */
        if (strcmp(cmd_argv[0], "help") == 0) {
            cmd_help();
        } else if (strcmp(cmd_argv[0], "exit") == 0) {
            int code = 0;
            if (cmd_argc > 1) {
                int parsed = atoi(cmd_argv[1]);
                if (parsed >= 0) code = parsed;
            }
            exit(code);
        } else if (strcmp(cmd_argv[0], "echo") == 0) {
            /* echo supports redirection via save/redirect/restore */
            if (redir_out_mode != REDIR_NONE || has_redir_in) {
                int saved_stdout = -1;
                int saved_stdin = -1;

                /* Save current stdout if redirecting output */
                if (redir_out_mode != REDIR_NONE) {
                    saved_stdout = dup(STDOUT_FILENO);
                }
                /* Save current stdin if redirecting input */
                if (has_redir_in) {
                    saved_stdin = dup(STDIN_FILENO);
                }

                /* Apply redirections */
                if (apply_redirections(redir_out, redir_out_mode,
                                       redir_in, has_redir_in) < 0) {
                    /* Restore on error */
                    if (saved_stdout >= 0) {
                        dup2(saved_stdout, STDOUT_FILENO);
                        close(saved_stdout);
                    }
                    if (saved_stdin >= 0) {
                        dup2(saved_stdin, STDIN_FILENO);
                        close(saved_stdin);
                    }
                    continue;
                }

                /* Run built-in */
                cmd_echo(cmd_argc, cmd_argv);

                /* Restore stdout */
                if (saved_stdout >= 0) {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
                /* Restore stdin */
                if (saved_stdin >= 0) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
            } else {
                cmd_echo(cmd_argc, cmd_argv);
            }
        } else if (strcmp(cmd_argv[0], "set") == 0) {
            cmd_set(cmd_argc, cmd_argv);
        } else if (strcmp(cmd_argv[0], "unset") == 0) {
            cmd_unset(cmd_argc, cmd_argv);
        } else if (strcmp(cmd_argv[0], "cd") == 0) {
            cmd_cd(cmd_argc, cmd_argv);
        } else {
            /* Try external program */
            run_external(cmd_argc, cmd_argv,
                         redir_out, redir_out_mode,
                         redir_in, has_redir_in);
        }
    }

    return 0;
}
