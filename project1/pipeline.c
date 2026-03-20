/*
 * pipeline.c
 * Demonstrates: fork(), execvp(), pipe(), and file I/O
 * Pipeline: ps aux | grep root  →  output saved to output.txt
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#define OUTPUT_FILE "output.txt"
#define READ_PREVIEW 512 /* bytes to preview from the file */

int main(void)
{
    int pipefd[2]; /* pipefd[0] = read end, pipefd[1] = write end */
    pid_t pid1, pid2;
    int outfd;

    /* 1. Create the pipe                                                   */
    if (pipe(pipefd) == -1)
    {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* 2. Open output file (created/truncated)                             */
    outfd = open(OUTPUT_FILE,
                 O_WRONLY | O_CREAT | O_TRUNC,
                 0644);
    if (outfd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    /* CHILD 1 — runs: ps aux                                              */
    /*           writes its stdout into the WRITE end of the pipe          */
    pid1 = fork();
    if (pid1 == -1)
    {
        perror("fork (child 1)");
        exit(EXIT_FAILURE);
    }

    if (pid1 == 0)
    { /* ---- child 1 ---- */
        /* Redirect stdout → pipe write end */
        if (dup2(pipefd[1], STDOUT_FILENO) == -1)
        {
            perror("dup2 child1");
            exit(EXIT_FAILURE);
        }
        /* Close both pipe ends (dup2 already copied what we need) */
        close(pipefd[0]);
        close(pipefd[1]);
        close(outfd); /* child 1 does not use the file */

        char *args[] = {"ps", "aux", NULL};
        execvp("ps", args);
        perror("execvp ps"); /* reached only on error */
        exit(EXIT_FAILURE);
    }

    /* CHILD 2 — runs: grep root                                           */
    /*           reads from the READ end of the pipe                       */
    /*           writes its stdout into output.txt                         */
    pid2 = fork();
    if (pid2 == -1)
    {
        perror("fork (child 2)");
        exit(EXIT_FAILURE);
    }

    if (pid2 == 0)
    { /* ---- child 2 ---- */
        /* Redirect stdin  → pipe read end  */
        if (dup2(pipefd[0], STDIN_FILENO) == -1)
        {
            perror("dup2 child2 stdin");
            exit(EXIT_FAILURE);
        }
        /* Redirect stdout → output file    */
        if (dup2(outfd, STDOUT_FILENO) == -1)
        {
            perror("dup2 child2 stdout");
            exit(EXIT_FAILURE);
        }
        close(pipefd[0]);
        close(pipefd[1]);
        close(outfd);

        char *args[] = {"grep", "root", NULL};
        execvp("grep", args);
        perror("execvp grep");
        exit(EXIT_FAILURE);
    }

    /* PARENT — closes pipe ends it does not use, waits for children       */
    close(pipefd[0]); /* parent does not read from pipe  */
    close(pipefd[1]); /* parent does not write to pipe   */
    /* NOTE: keep outfd open; we will read from the file after children
     * finish, but we re-open it for reading below.                       */
    close(outfd);

    /* Wait for both children */
    int status;
    waitpid(pid1, &status, 0);
    printf("[Parent] Child 1 (ps aux)   exited with status %d\n",
           WEXITSTATUS(status));

    waitpid(pid2, &status, 0);
    printf("[Parent] Child 2 (grep root) exited with status %d\n",
           WEXITSTATUS(status));

    /* PARENT — read and display a preview of output.txt                  */
    int readfd = open(OUTPUT_FILE, O_RDONLY);
    if (readfd == -1)
    {
        perror("open for read");
        exit(EXIT_FAILURE);
    }

    char buf[READ_PREVIEW + 1];
    ssize_t n = read(readfd, buf, READ_PREVIEW);
    if (n < 0)
    {
        perror("read");
        close(readfd);
        exit(EXIT_FAILURE);
    }
    buf[n] = '\0';

    printf("\n[Parent] Preview of '%s' (%zd bytes):\n", OUTPUT_FILE, n);
    printf("--------------------------------------------\n");
    printf("%s", buf);
    printf("--------------------------------------------\n");
    printf("[Parent] Full output saved to '%s'.\n", OUTPUT_FILE);

    close(readfd);
    return 0;
}

