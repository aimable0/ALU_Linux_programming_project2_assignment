/*
 * copy_stdio.c — Version 2: Standard I/O library (fread/fwrite)
 * Usage: ./copy_stdio <source> <destination>
 * Copies file using fread()/fwrite() with libc's internal buffering.
 * Measures and prints wall-clock execution time.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BUFFER_SIZE (64 * 1024)   /* 64 KB — matches Version 1 for fair comparison */

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source> <destination>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    /* ── Open source ──────────────────────────────────────────────────── */
    FILE *src = fopen(argv[1], "rb");
    if (!src) {
        perror("fopen source");
        exit(EXIT_FAILURE);
    }

    /* ── Open destination ─────────────────────────────────────────────── */
    FILE *dst = fopen(argv[2], "wb");
    if (!dst) {
        perror("fopen destination");
        fclose(src);
        exit(EXIT_FAILURE);
    }

    /*
     * Optionally set a large stream buffer explicitly.
     * libc usually picks 4–8 KB by default; matching our 64 KB
     * block size gives a fairer comparison against Version 1.
     */
    char *src_buf = malloc(BUFFER_SIZE);
    char *dst_buf = malloc(BUFFER_SIZE);
    if (!src_buf || !dst_buf) {
        perror("malloc");
        fclose(src); fclose(dst);
        exit(EXIT_FAILURE);
    }
    setvbuf(src, src_buf, _IOFBF, BUFFER_SIZE);
    setvbuf(dst, dst_buf, _IOFBF, BUFFER_SIZE);

    /* ── Allocate user-space transfer buffer ──────────────────────────── */
    char *buf = malloc(BUFFER_SIZE);
    if (!buf) {
        perror("malloc transfer buf");
        fclose(src); fclose(dst);
        exit(EXIT_FAILURE);
    }

    /* ── Start timer ──────────────────────────────────────────────────── */
    struct timespec t_start, t_end;
    clock_gettime(CLOCK_MONOTONIC, &t_start);

    /* ── Copy loop: fread → fwrite until EOF ──────────────────────────── */
    size_t bytes_read;
    long long total_bytes = 0;
    long long fread_calls = 0;

    while ((bytes_read = fread(buf, 1, BUFFER_SIZE, src)) > 0) {
        size_t written = fwrite(buf, 1, bytes_read, dst);
        if (written != bytes_read) {
            fprintf(stderr, "fwrite: short write (wrote %zu of %zu)\n",
                    written, bytes_read);
            free(buf); free(src_buf); free(dst_buf);
            fclose(src); fclose(dst);
            exit(EXIT_FAILURE);
        }
        total_bytes += (long long)bytes_read;
        fread_calls++;
    }

    if (ferror(src)) {
        perror("fread");
        free(buf); free(src_buf); free(dst_buf);
        fclose(src); fclose(dst);
        exit(EXIT_FAILURE);
    }

    /* ── Flush destination buffer to kernel ───────────────────────────── */
    if (fflush(dst) != 0) {
        perror("fflush");
    }

    /* ── Stop timer ───────────────────────────────────────────────────── */
    clock_gettime(CLOCK_MONOTONIC, &t_end);

    double elapsed = (t_end.tv_sec  - t_start.tv_sec) +
                     (t_end.tv_nsec - t_start.tv_nsec) / 1e9;

    free(buf); free(src_buf); free(dst_buf);
    fclose(src);
    fclose(dst);

    /* ── Report ───────────────────────────────────────────────────────── */
    printf("=== Version 2: Standard I/O fread()/fwrite() ===\n");
    printf("  Source       : %s\n", argv[1]);
    printf("  Destination  : %s\n", argv[2]);
    printf("  Bytes copied : %lld bytes (%.2f MB)\n",
           total_bytes, total_bytes / (1024.0 * 1024.0));
    printf("  Buffer size  : %d KB\n", BUFFER_SIZE / 1024);
    printf("  fread calls  : %lld\n", fread_calls);
    printf("  Elapsed time : %.4f seconds\n", elapsed);
    printf("  Throughput   : %.2f MB/s\n",
           (total_bytes / (1024.0 * 1024.0)) / elapsed);

    return 0;
}

