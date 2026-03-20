/*
 * copy_syscall.c — Version 1: Low-level system calls (read/write)
 * Usage: ./copy_syscall <source> <destination>
 * Copies file using raw read()/write() with a fixed buffer.
 * Measures and prints wall-clock execution time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BUFFER_SIZE (64 * 1024)   /* 64 KB — one kernel page cluster */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* ── Open source (read-only) ──────────────────────────────────────── */
    int src_fd = open(argv[1], O_RDONLY);
    if (src_fd == -1) {
        perror("open source");
        exit(EXIT_FAILURE);
    }

    /* ── Open/create destination (write, truncate) ────────────────────── */
    int dst_fd = open(argv[2],
                      O_WRONLY | O_CREAT | O_TRUNC,
                      0644);
    if (dst_fd == -1) {
        perror("open destination");
        close(src_fd);
        exit(EXIT_FAILURE);
    }

    /* ── Allocate buffer on the heap ──────────────────────────────────── */
    char *buf = malloc(BUFFER_SIZE);
    if (!buf) {
        perror("malloc");
        close(src_fd);
        close(dst_fd);
        exit(EXIT_FAILURE);
    }

    /* ── Start timer ──────────────────────────────────────────────────── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* ── Copy loop: read → write until EOF ───────────────────────────── */
    ssize_t bytes_read;
    long long total_bytes = 0;
    long long syscall_pairs = 0;

    while ((bytes_read = read(src_fd, buf, BUFFER_SIZE)) > 0) {
        ssize_t written = 0;
        while (written < bytes_read) {
            ssize_t w = write(dst_fd, buf + written, bytes_read - written);
            if (w == -1) {
                perror("write");
                free(buf);
                close(src_fd);
                close(dst_fd);
                exit(EXIT_FAILURE);
            }
            written += w;
        }
        total_bytes += bytes_read;
        syscall_pairs++;
    }

    if (bytes_read == -1) {
        perror("read");
        free(buf);
        close(src_fd);
        close(dst_fd);
        exit(EXIT_FAILURE);
    }

    /* ── Stop timer ───────────────────────────────────────────────────── */
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed = (t_end.tv_sec  - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    free(buf);
    close(src_fd);
    close(dst_fd);

    /* ── Report ───────────────────────────────────────────────────────── */
    printf("=== Version 1: Low-level read()/write() ===\n");
    printf("  Source       : %s\n", argv[1]);
    printf("  Destination  : %s\n", argv[2]);
    printf("  Bytes copied : %lld bytes (%.2f MB)\n",
           total_bytes, total_bytes / (1024.0 * 1024.0));
    printf("  Buffer size  : %d KB\n", BUFFER_SIZE / 1024);
    printf("  read/write   : %lld pairs\n", syscall_pairs);
    printf("  Elapsed time : %.4f seconds\n", elapsed);
    printf("  Throughput   : %.2f MB/s\n",
           (total_bytes / (1024.0 * 1024.0)) / elapsed);

    return 0;
}

