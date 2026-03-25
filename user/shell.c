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
 * Background: cmd & (fork+exec without waiting)
 *
 * Scripting: sh scriptfile, /etc/rc auto-sourced on boot
 * Comments: # at start of line
 * Flow control: if/then/else/fi, while/do/done, for/in/do/done
 */

#include "libc.h"

#define MAX_CMD_LEN      128
#define MAX_ARGS         8
#define MAX_PATH_LEN     64
#define MAX_PIPE_STAGES  4
#define MAX_BLOCK_LINES  16

/* Redirection modes */
#define REDIR_NONE    0
#define REDIR_TRUNC   1   /* >  */
#define REDIR_APPEND  2   /* >> */

/* ================================================================
 * Global script state
 * ================================================================ */

static int script_fd = -1;       /* -1 = interactive, >= 0 = reading from file */
static int non_interactive = 0;  /* 1 = exit on EOF (script argument mode) */

/* ================================================================
 * Line reading
 * ================================================================ */

/*
 * Read one line from a file descriptor, byte by byte.
 * Stops at newline or EOF. Does NOT include the newline in buf.
 * Returns number of chars read, or -1 on EOF (no data).
 */
static int read_line(int fd, char *buf, int size)
{
    int n = 0;
    char c;
    int r;

    while (n < size - 1) {
        r = read(fd, &c, 1);
        if (r <= 0) {
            /* EOF */
            if (n == 0) return -1;  /* No data at all */
            break;
        }
        if (c == '\n') break;
        buf[n++] = c;
    }
    buf[n] = '\0';
    return n;
}

/*
 * Get next command line from appropriate source.
 * In interactive mode: prints prompt, reads from stdin via gets().
 * In script mode: reads from script_fd via read_line().
 * Returns 0 on success, -1 on EOF.
 */
static int get_line(char *buf, int size)
{
    if (script_fd >= 0) {
        return read_line(script_fd, buf, size) >= 0 ? 0 : -1;
    }

    /* Interactive mode */
    char *cwd = getenv("CWD");
    if (cwd) write(STDOUT_FILENO, cwd, strlen(cwd));
    write(STDOUT_FILENO, "$ ", 2);
    gets(buf, size);

    /* gets() returns empty string on EOF from console — treat as valid */
    return 0;
}

/* ================================================================
 * String utilities
 * ================================================================ */

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
 * Skip leading whitespace and check if line is a comment or empty.
 * Returns pointer to first non-whitespace char.
 */
static const char *skip_ws(const char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

/*
 * Check if a string starts with a given keyword followed by space/tab/NUL.
 * Returns 1 if match, 0 otherwise.
 */
static int starts_with_keyword(const char *line, const char *kw)
{
    const char *s = skip_ws(line);
    int len = strlen(kw);
    if (strncmp(s, kw, len) != 0) return 0;
    return (s[len] == '\0' || s[len] == ' ' || s[len] == '\t');
}

/* ================================================================
 * Parsing
 * ================================================================ */

/*
 * Parse command line into argv array.
 * Modifies 'line' in place by replacing spaces with NULs.
 * Returns argc (number of arguments).
 */
static int parse_cmd(char *line, char *argv[], int max_args)
{
    int argc = 0;
    int in_word = 0;

    while (*line && argc < max_args - 1) {
        if (*line == ' ' || *line == '\t') {
            if (in_word) {
                *line = '\0';
                in_word = 0;
            }
        } else {
            if (!in_word) {
                argv[argc++] = line;
                in_word = 1;
            }
        }
        line++;
    }
    argv[argc] = (char *)0;
    return argc;
}

/*
 * Parse redirection operators from argv.
 * Scans for >, >>, < tokens and extracts filenames.
 * Removes redirection tokens from argv, adjusting argc.
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
        if (argv[i][0] == '>' && argv[i][1] == '>' && argv[i][2] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_out, argv[i + 1], MAX_PATH_LEN - 1);
                redir_out[MAX_PATH_LEN - 1] = '\0';
                *redir_out_mode = REDIR_APPEND;
                i += 2;
                continue;
            }
        } else if (argv[i][0] == '>' && argv[i][1] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_out, argv[i + 1], MAX_PATH_LEN - 1);
                redir_out[MAX_PATH_LEN - 1] = '\0';
                *redir_out_mode = REDIR_TRUNC;
                i += 2;
                continue;
            }
        } else if (argv[i][0] == '<' && argv[i][1] == '\0') {
            if (i + 1 < argc) {
                strncpy(redir_in, argv[i + 1], MAX_PATH_LEN - 1);
                redir_in[MAX_PATH_LEN - 1] = '\0';
                *has_redir_in = 1;
                i += 2;
                continue;
            }
        }

        argv[new_argc++] = argv[i];
        i++;
    }
    argv[new_argc] = (char *)0;
    return new_argc;
}

/* ================================================================
 * I/O redirection helpers
 * ================================================================ */

static int apply_redirections(const char *redir_out, int redir_out_mode,
                              const char *redir_in, int has_redir_in)
{
    int fd;

    if (has_redir_in) {
        fd = open(redir_in, O_RDONLY);
        if (fd < 0) {
            printf("sh: %s: No such file\n", redir_in);
            return -1;
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }

    if (redir_out_mode == REDIR_TRUNC) {
        fd = open(redir_out, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0) {
            printf("sh: %s: Cannot open for writing\n", redir_out);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    } else if (redir_out_mode == REDIR_APPEND) {
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

/* ================================================================
 * Built-in commands
 * ================================================================ */

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
    puts("Background:  cmd &  (run without waiting)");
    puts("Scripting:   sh scriptfile");
    puts("Flow:        if/then/else/fi, while/do/done, for/in/do/done");
    puts("Comments:    # at start of line");
    puts("Variables:   $VAR or ${VAR} are expanded in all arguments.");
    puts("PATH is used to locate commands (default: /bin).");
}

static void cmd_echo(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (i > 1) putchar(' ');
        write(STDOUT_FILENO, argv[i], strlen(argv[i]));
    }
    putchar('\n');
}

static void cmd_unset(int argc, char *argv[])
{
    int i;
    for (i = 1; i < argc; i++) {
        if (unsetenv(argv[i]) < 0) {
            printf("unset: %s: not set\n", argv[i]);
        }
    }
}

static void cmd_set(int argc, char *argv[])
{
    int i;

    if (argc == 1) {
        int count = env_count();
        char buf[64];
        for (i = 0; i < count; i++) {
            getenv_entry(i, buf, sizeof(buf));
            puts(buf);
        }
        return;
    }

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

static void cmd_cd(int argc, char *argv[])
{
    const char *path = (argc > 1) ? argv[1] : "/";
    if (chdir(path) < 0) {
        printf("cd: %s: not found\n", path);
    }
}

/* ================================================================
 * External command execution
 * ================================================================ */

static void exec_with_path(char *argv[])
{
    char path[MAX_PATH_LEN];

    if (strchr(argv[0], '/') != (char *)0) {
        exec(argv[0], argv);
        printf("sh: %s: not found\n", argv[0]);
        return;
    }

    char *path_var = getenv("PATH");
    if (path_var == (char *)0) {
        path_var = "/bin";
    }

    const char *p = path_var;
    while (*p) {
        int dlen = 0;
        while (p[dlen] && p[dlen] != ':') dlen++;

        if (dlen > 0 && dlen < (int)sizeof(path) - 2) {
            memcpy(path, p, dlen);
            path[dlen] = '/';
            strncpy(path + dlen + 1, argv[0], sizeof(path) - dlen - 2);
            path[sizeof(path) - 1] = '\0';

            exec(path, argv);
        }

        p += dlen;
        if (*p == ':') p++;
    }

    printf("sh: %s: command not found\n", argv[0]);
}

static void run_external(int argc, char *argv[],
                         const char *redir_out, int redir_out_mode,
                         const char *redir_in, int has_redir_in,
                         int background)
{
    int pid;

    (void)argc;

    pid = fork();
    if (pid < 0) {
        printf("sh: fork failed\n");
        return;
    }

    if (pid == 0) {
        if (apply_redirections(redir_out, redir_out_mode,
                               redir_in, has_redir_in) < 0) {
            exit(1);
        }
        exec_with_path(argv);
        exit(127);
    }

    if (background) {
        printf("[%d]\n", pid);
    } else {
        wait();
    }
}

static void run_multi_pipeline(int n_stages, char **argv_s[],
                                char rout_s[][MAX_PATH_LEN],
                                int rout_mode_s[],
                                char rin_s[][MAX_PATH_LEN],
                                int has_rin_s[],
                                int background)
{
    int pfd[2];
    int prev_read_fd = -1;
    int pid;
    int i, forked;

    forked = 0;
    for (i = 0; i < n_stages; i++) {
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
            if (prev_read_fd >= 0) {
                dup2(prev_read_fd, STDIN_FILENO);
                close(prev_read_fd);
            }

            if (i < n_stages - 1) {
                close(pfd[0]);
                dup2(pfd[1], STDOUT_FILENO);
                close(pfd[1]);
            }

            if (apply_redirections(rout_s[i], rout_mode_s[i],
                                   rin_s[i], has_rin_s[i]) < 0) {
                exit(1);
            }

            exec_with_path(argv_s[i]);
            exit(127);
        }

        forked++;

        if (prev_read_fd >= 0)
            close(prev_read_fd);

        if (i < n_stages - 1) {
            close(pfd[1]);
            prev_read_fd = pfd[0];
        }
    }

    if (prev_read_fd >= 0)
        close(prev_read_fd);

    if (background) {
        printf("[pipeline in background]\n");
    } else {
        for (i = 0; i < forked; i++)
            wait();
    }
}

/* ================================================================
 * Flow control: block collection and execution
 * ================================================================ */

/* Forward declaration — execute_line and execute_block are mutually recursive */
static int execute_line(const char *raw_line);

/*
 * Collect lines from input until a terminator keyword at nesting depth 0.
 * end1 is the primary terminator (e.g., "fi", "done").
 * end2 is an optional secondary terminator (e.g., "else") or NULL.
 * Returns: 1 = hit end1, 2 = hit end2, 0 = EOF/error, -1 = overflow.
 * On return, buf contains the collected lines and *count is set.
 */
static int collect_block(char buf[][MAX_CMD_LEN], int *count, int max,
                         const char *end1, const char *end2)
{
    int depth = 0;
    char line[MAX_CMD_LEN];
    *count = 0;

    while (1) {
        if (get_line(line, MAX_CMD_LEN) < 0) return 0;  /* EOF */

        const char *trimmed = skip_ws(line);

        /* Track nesting: if/while/for increase depth */
        if (starts_with_keyword(trimmed, "if") ||
            starts_with_keyword(trimmed, "while") ||
            starts_with_keyword(trimmed, "for")) {
            depth++;
        }

        /* Check terminators at depth 0 */
        if (depth == 0) {
            if (starts_with_keyword(trimmed, end1)) return 1;
            if (end2 && starts_with_keyword(trimmed, end2)) return 2;
        }

        /* fi/done decrease depth */
        if (starts_with_keyword(trimmed, "fi") ||
            starts_with_keyword(trimmed, "done")) {
            depth--;
        }

        /* Store line in block */
        if (*count >= max) {
            printf("sh: block too large (max %d lines)\n", max);
            return -1;
        }
        strncpy(buf[*count], line, MAX_CMD_LEN - 1);
        buf[*count][MAX_CMD_LEN - 1] = '\0';
        (*count)++;
    }
}

/*
 * Collect lines from a pre-existing array (for nested flow control).
 * Reads from lines[*idx] onwards, advancing *idx.
 * Returns: 1 = hit end1, 2 = hit end2, 0 = ran out of lines, -1 = overflow.
 */
static int collect_block_from_array(char src[][MAX_CMD_LEN], int src_count, int *idx,
                                    char buf[][MAX_CMD_LEN], int *count, int max,
                                    const char *end1, const char *end2)
{
    int depth = 0;
    *count = 0;

    while (*idx < src_count) {
        const char *trimmed = skip_ws(src[*idx]);

        if (starts_with_keyword(trimmed, "if") ||
            starts_with_keyword(trimmed, "while") ||
            starts_with_keyword(trimmed, "for")) {
            depth++;
        }

        if (depth == 0) {
            if (starts_with_keyword(trimmed, end1)) { (*idx)++; return 1; }
            if (end2 && starts_with_keyword(trimmed, end2)) { (*idx)++; return 2; }
        }

        if (starts_with_keyword(trimmed, "fi") ||
            starts_with_keyword(trimmed, "done")) {
            depth--;
        }

        if (*count >= max) {
            printf("sh: block too large (max %d lines)\n", max);
            return -1;
        }
        strncpy(buf[*count], src[*idx], MAX_CMD_LEN - 1);
        buf[*count][MAX_CMD_LEN - 1] = '\0';
        (*count)++;
        (*idx)++;
    }
    return 0;
}

/*
 * Execute an array of command lines, handling nested flow control.
 */
static void execute_block(char lines[][MAX_CMD_LEN], int count)
{
    int i = 0;

    while (i < count) {
        const char *trimmed = skip_ws(lines[i]);

        /* Skip empty / comment lines */
        if (*trimmed == '\0' || *trimmed == '#') { i++; continue; }

        /* --- if/then/else/fi --- */
        if (starts_with_keyword(trimmed, "if")) {
            /* Execute the condition (rest of line after "if ") */
            const char *cond = trimmed + 2;
            while (*cond == ' ' || *cond == '\t') cond++;
            if (*cond) execute_line(cond);
            char *exitcode = getenv("?");
            int cond_ok = (exitcode && exitcode[0] == '0' && exitcode[1] == '\0');

            i++;  /* past 'if' line */

            /* Expect 'then' */
            if (i < count && starts_with_keyword(skip_ws(lines[i]), "then")) {
                i++;  /* past 'then' */
            } else {
                printf("sh: expected 'then'\n");
                continue;
            }

            /* Collect then-block */
            char then_block[MAX_BLOCK_LINES][MAX_CMD_LEN];
            int then_count = 0;
            int r = collect_block_from_array(lines, count, &i,
                                             then_block, &then_count, MAX_BLOCK_LINES,
                                             "fi", "else");
            if (r == 2) {
                /* Hit 'else' — collect else-block */
                char else_block[MAX_BLOCK_LINES][MAX_CMD_LEN];
                int else_count = 0;
                collect_block_from_array(lines, count, &i,
                                         else_block, &else_count, MAX_BLOCK_LINES,
                                         "fi", (char *)0);
                if (cond_ok)
                    execute_block(then_block, then_count);
                else
                    execute_block(else_block, else_count);
            } else {
                /* Hit 'fi' directly */
                if (cond_ok)
                    execute_block(then_block, then_count);
            }
            continue;
        }

        /* --- while/do/done --- */
        if (starts_with_keyword(trimmed, "while")) {
            const char *cond = trimmed + 5;
            while (*cond == ' ' || *cond == '\t') cond++;
            char cond_str[MAX_CMD_LEN];
            strncpy(cond_str, cond, MAX_CMD_LEN - 1);
            cond_str[MAX_CMD_LEN - 1] = '\0';

            i++;  /* past 'while' line */

            /* Expect 'do' */
            if (i < count && starts_with_keyword(skip_ws(lines[i]), "do")) {
                i++;  /* past 'do' */
            } else {
                printf("sh: expected 'do'\n");
                continue;
            }

            /* Collect body */
            char body[MAX_BLOCK_LINES][MAX_CMD_LEN];
            int body_count = 0;
            int save_i = i;
            collect_block_from_array(lines, count, &i,
                                     body, &body_count, MAX_BLOCK_LINES,
                                     "done", (char *)0);

            /* Loop */
            while (1) {
                execute_line(cond_str);
                char *exitcode = getenv("?");
                if (!(exitcode && exitcode[0] == '0' && exitcode[1] == '\0'))
                    break;
                execute_block(body, body_count);
            }
            continue;
        }

        /* --- for/in/do/done --- */
        if (starts_with_keyword(trimmed, "for")) {
            /* Parse: for VAR in arg1 arg2 ... */
            char for_line[MAX_CMD_LEN];
            strncpy(for_line, trimmed, MAX_CMD_LEN - 1);
            for_line[MAX_CMD_LEN - 1] = '\0';

            char *for_argv[MAX_ARGS];
            int for_argc = parse_cmd(for_line + 3, for_argv, MAX_ARGS);  /* skip "for" */

            if (for_argc < 3 || strcmp(for_argv[1], "in") != 0) {
                printf("sh: usage: for VAR in args...\n");
                i++;
                continue;
            }

            char varname[32];
            strncpy(varname, for_argv[0], sizeof(varname) - 1);
            varname[sizeof(varname) - 1] = '\0';

            /* Collect the word list before parse_cmd destroys for_line */
            char words[MAX_ARGS][MAX_CMD_LEN];
            int nwords = 0;
            int w;
            for (w = 2; w < for_argc && nwords < MAX_ARGS; w++) {
                strncpy(words[nwords], for_argv[w], MAX_CMD_LEN - 1);
                words[nwords][MAX_CMD_LEN - 1] = '\0';
                nwords++;
            }

            i++;  /* past 'for' line */

            /* Expect 'do' */
            if (i < count && starts_with_keyword(skip_ws(lines[i]), "do")) {
                i++;  /* past 'do' */
            } else {
                printf("sh: expected 'do'\n");
                continue;
            }

            /* Collect body */
            char body[MAX_BLOCK_LINES][MAX_CMD_LEN];
            int body_count = 0;
            collect_block_from_array(lines, count, &i,
                                     body, &body_count, MAX_BLOCK_LINES,
                                     "done", (char *)0);

            /* Iterate */
            for (w = 0; w < nwords; w++) {
                setenv(varname, words[w]);
                execute_block(body, body_count);
            }
            continue;
        }

        /* Regular line */
        execute_line(lines[i]);
        i++;
    }
}

/* ================================================================
 * Top-level flow control handlers (read from input, not array)
 * ================================================================ */

/*
 * Handle 'if' at top level — reads then/else/fi from input.
 * 'cond' is the condition command (text after "if ").
 */
static void handle_if(const char *cond)
{
    char line[MAX_CMD_LEN];
    int r;

    /* Execute condition */
    if (*cond) execute_line(cond);
    char *exitcode = getenv("?");
    int cond_ok = (exitcode && exitcode[0] == '0' && exitcode[1] == '\0');

    /* Expect 'then' */
    if (get_line(line, MAX_CMD_LEN) < 0 || !starts_with_keyword(skip_ws(line), "then")) {
        printf("sh: expected 'then'\n");
        return;
    }

    /* Collect then-block */
    char then_block[MAX_BLOCK_LINES][MAX_CMD_LEN];
    int then_count = 0;
    r = collect_block(then_block, &then_count, MAX_BLOCK_LINES, "fi", "else");

    if (r == 2) {
        /* Hit 'else' — collect else-block */
        char else_block[MAX_BLOCK_LINES][MAX_CMD_LEN];
        int else_count = 0;
        collect_block(else_block, &else_count, MAX_BLOCK_LINES, "fi", (char *)0);
        if (cond_ok)
            execute_block(then_block, then_count);
        else
            execute_block(else_block, else_count);
    } else if (r == 1) {
        /* Hit 'fi' directly */
        if (cond_ok)
            execute_block(then_block, then_count);
    } else {
        printf("sh: unexpected end of input in 'if'\n");
    }
}

/*
 * Handle 'while' at top level.
 * 'cond' is the condition command (text after "while ").
 */
static void handle_while(const char *cond)
{
    char line[MAX_CMD_LEN];
    char cond_str[MAX_CMD_LEN];
    strncpy(cond_str, cond, MAX_CMD_LEN - 1);
    cond_str[MAX_CMD_LEN - 1] = '\0';

    /* Expect 'do' */
    if (get_line(line, MAX_CMD_LEN) < 0 || !starts_with_keyword(skip_ws(line), "do")) {
        printf("sh: expected 'do'\n");
        return;
    }

    /* Collect body */
    char body[MAX_BLOCK_LINES][MAX_CMD_LEN];
    int body_count = 0;
    int r = collect_block(body, &body_count, MAX_BLOCK_LINES, "done", (char *)0);
    if (r != 1) {
        printf("sh: unexpected end of input in 'while'\n");
        return;
    }

    /* Loop */
    while (1) {
        execute_line(cond_str);
        char *exitcode = getenv("?");
        if (!(exitcode && exitcode[0] == '0' && exitcode[1] == '\0'))
            break;
        execute_block(body, body_count);
    }
}

/*
 * Handle 'for' at top level.
 * 'rest' is everything after "for " (e.g., "x in a b c").
 */
static void handle_for(const char *rest)
{
    char line[MAX_CMD_LEN];
    char rest_buf[MAX_CMD_LEN];
    strncpy(rest_buf, rest, MAX_CMD_LEN - 1);
    rest_buf[MAX_CMD_LEN - 1] = '\0';

    char *for_argv[MAX_ARGS];
    int for_argc = parse_cmd(rest_buf, for_argv, MAX_ARGS);

    if (for_argc < 3 || strcmp(for_argv[1], "in") != 0) {
        printf("sh: usage: for VAR in args...\n");
        return;
    }

    char varname[32];
    strncpy(varname, for_argv[0], sizeof(varname) - 1);
    varname[sizeof(varname) - 1] = '\0';

    /* Save word list before parse_cmd corrupts rest_buf */
    char words[MAX_ARGS][MAX_CMD_LEN];
    int nwords = 0;
    int w;
    for (w = 2; w < for_argc && nwords < MAX_ARGS; w++) {
        strncpy(words[nwords], for_argv[w], MAX_CMD_LEN - 1);
        words[nwords][MAX_CMD_LEN - 1] = '\0';
        nwords++;
    }

    /* Expect 'do' */
    if (get_line(line, MAX_CMD_LEN) < 0 || !starts_with_keyword(skip_ws(line), "do")) {
        printf("sh: expected 'do'\n");
        return;
    }

    /* Collect body */
    char body[MAX_BLOCK_LINES][MAX_CMD_LEN];
    int body_count = 0;
    int r = collect_block(body, &body_count, MAX_BLOCK_LINES, "done", (char *)0);
    if (r != 1) {
        printf("sh: unexpected end of input in 'for'\n");
        return;
    }

    /* Iterate */
    for (w = 0; w < nwords; w++) {
        setenv(varname, words[w]);
        execute_block(body, body_count);
    }
}

/* ================================================================
 * Core command dispatch — execute a single command line
 * ================================================================ */

/*
 * Execute a single command line.
 * Handles variable expansion, pipes, redirections, built-ins, externals.
 * Returns 0 normally, -1 if 'exit' was invoked.
 */
static int execute_line(const char *raw_line)
{
    char expanded[MAX_CMD_LEN];
    char *cmd_argv[MAX_ARGS];
    int cmd_argc;
    char redir_out[MAX_PATH_LEN];
    char redir_in[MAX_PATH_LEN];
    int redir_out_mode;
    int has_redir_in;

    const char *trimmed = skip_ws(raw_line);

    /* Skip empty / comment lines */
    if (*trimmed == '\0' || *trimmed == '#') return 0;

    /* Expand $VAR references */
    expand_vars(trimmed, expanded, MAX_CMD_LEN);

    /* --- Flow control keywords --- */
    if (starts_with_keyword(expanded, "if")) {
        const char *cond = expanded + 2;
        while (*cond == ' ' || *cond == '\t') cond++;
        handle_if(cond);
        return 0;
    }
    if (starts_with_keyword(expanded, "while")) {
        const char *cond = expanded + 5;
        while (*cond == ' ' || *cond == '\t') cond++;
        handle_while(cond);
        return 0;
    }
    if (starts_with_keyword(expanded, "for")) {
        const char *rest = expanded + 3;
        while (*rest == ' ' || *rest == '\t') rest++;
        handle_for(rest);
        return 0;
    }

    /* --- Pipe handling --- */
    if (strchr(expanded, '|') != (char *)0) {
        char seg_bufs[MAX_PIPE_STAGES][MAX_CMD_LEN];
        char *seg_argv[MAX_PIPE_STAGES][MAX_ARGS];
        int seg_argc[MAX_PIPE_STAGES];
        char seg_rout[MAX_PIPE_STAGES][MAX_PATH_LEN];
        char seg_rin[MAX_PIPE_STAGES][MAX_PATH_LEN];
        int seg_rout_mode[MAX_PIPE_STAGES];
        int seg_has_rin[MAX_PIPE_STAGES];
        int n_stages = 0;
        int ok = 1;

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
            return 0;
        }

        int pipe_bg = 0;
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
            if (ok && n_stages > 0) {
                int last = n_stages - 1;
                if (seg_argc[last] > 0 &&
                    strcmp(seg_argv[last][seg_argc[last] - 1], "&") == 0) {
                    pipe_bg = 1;
                    seg_argv[last][--seg_argc[last]] = (char *)0;
                    if (seg_argc[last] == 0) { ok = 0; }
                }
            }
        }
        if (!ok) return 0;

        {
            int s;
            char **argv_arr[MAX_PIPE_STAGES];
            for (s = 0; s < n_stages; s++)
                argv_arr[s] = seg_argv[s];
            run_multi_pipeline(n_stages, argv_arr,
                               seg_rout, seg_rout_mode,
                               seg_rin, seg_has_rin,
                               pipe_bg);
        }
        return 0;
    }

    /* --- Single command --- */
    cmd_argc = parse_cmd(expanded, cmd_argv, MAX_ARGS);
    if (cmd_argc == 0) return 0;

    /* Check for trailing '&' — background execution */
    int background = 0;
    if (cmd_argc > 0 && strcmp(cmd_argv[cmd_argc - 1], "&") == 0) {
        background = 1;
        cmd_argv[--cmd_argc] = (char *)0;
    }
    if (cmd_argc == 0) return 0;

    /* Extract redirections from argv */
    cmd_argc = parse_redirections(cmd_argc, cmd_argv,
                                  redir_out, &redir_out_mode,
                                  redir_in, &has_redir_in);
    if (cmd_argc == 0) return 0;

    /* --- Built-in commands --- */
    if (strcmp(cmd_argv[0], "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd_argv[0], "exit") == 0) {
        int code = 0;
        if (cmd_argc > 1) {
            int parsed = atoi(cmd_argv[1]);
            if (parsed >= 0) code = parsed;
        }
        exit(code);
        /* Not reached */
    } else if (strcmp(cmd_argv[0], "echo") == 0) {
        /* echo supports redirection via save/redirect/restore */
        if (redir_out_mode != REDIR_NONE || has_redir_in) {
            int saved_stdout = -1;
            int saved_stdin = -1;

            if (redir_out_mode != REDIR_NONE) {
                saved_stdout = dup(STDOUT_FILENO);
            }
            if (has_redir_in) {
                saved_stdin = dup(STDIN_FILENO);
            }

            if (apply_redirections(redir_out, redir_out_mode,
                                   redir_in, has_redir_in) < 0) {
                if (saved_stdout >= 0) {
                    dup2(saved_stdout, STDOUT_FILENO);
                    close(saved_stdout);
                }
                if (saved_stdin >= 0) {
                    dup2(saved_stdin, STDIN_FILENO);
                    close(saved_stdin);
                }
                return 0;
            }

            cmd_echo(cmd_argc, cmd_argv);

            if (saved_stdout >= 0) {
                dup2(saved_stdout, STDOUT_FILENO);
                close(saved_stdout);
            }
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
        /* External program */
        run_external(cmd_argc, cmd_argv,
                     redir_out, redir_out_mode,
                     redir_in, has_redir_in,
                     background);
    }

    return 0;
}

/* ================================================================
 * Main
 * ================================================================ */

int main(int argc, char *argv[])
{
    char line[MAX_CMD_LEN];

    /* Set default environment if PATH is not already set */
    if (getenv("PATH") == (char *)0) {
        setenv("PATH", "/bin");
    }

    /* Check for script argument: sh scriptfile */
    if (argc >= 2) {
        script_fd = open(argv[1], O_RDONLY);
        if (script_fd < 0) {
            printf("sh: %s: cannot open\n", argv[1]);
            return 1;
        }
        non_interactive = 1;
    }

    /* Source /etc/rc on boot (interactive mode only) */
    if (!non_interactive) {
        int rc_fd = open("/etc/rc", O_RDONLY);
        if (rc_fd >= 0) {
            script_fd = rc_fd;
            /* Run command loop in script mode; will switch to interactive on EOF */
            while (get_line(line, MAX_CMD_LEN) == 0) {
                execute_line(line);
            }
            close(script_fd);
            script_fd = -1;
        }
    }

    /* Main command loop */
    for (;;) {
        if (get_line(line, MAX_CMD_LEN) < 0) {
            /* EOF */
            if (non_interactive) {
                if (script_fd >= 0) close(script_fd);
                return 0;
            }
            /* Interactive EOF — shouldn't happen with console, but handle it */
            break;
        }

        execute_line(line);
    }

    return 0;
}
