/*
 * search.c — Multi-threaded keyword search across multiple text files
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>

/* ── Constants ──────────────────────────────────────────────────────────── */
#define READ_CHUNK  (64 * 1024)   /* 64 KB fread buffer per chunk */

/* ── Shared work queue ──────────────────────────────────────────────────── */
typedef struct {
    const char    **files;
    int             num_files;
    int             next_index;      /* next unclaimed file — protected below */
    pthread_mutex_t queue_mutex;
} work_queue_t;

/* ── Shared output state ────────────────────────────────────────────────── */
typedef struct {
    FILE           *fp;
    pthread_mutex_t write_mutex;
    long            total_hits;
} output_state_t;

/* ── Per-thread arguments ───────────────────────────────────────────────── */
typedef struct {
    int             thread_id;
    const char     *keyword;
    int             kw_len;
    work_queue_t   *queue;
    output_state_t *out;
} thread_args_t;

/* ──────────────────────────────────────────────────────────────────────────
 * count_keyword_in_file()
 *
 * Reads the file in 64 KB chunks via fread(). A sliding overlap of
 * (kw_len - 1) bytes is kept between consecutive chunks so that keywords
 * straddling a chunk boundary are not missed.
 * ────────────────────────────────────────────────────────────────────────── */
static long count_keyword_in_file(const char *path,
                                  const char *keyword,
                                  int         kw_len)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) { perror(path); return -1; }

    int    buf_size = READ_CHUNK + kw_len;   /* extra for overlap + NUL */
    char  *buf      = malloc(buf_size + 1);
    if (!buf) { fclose(fp); return -1; }

    long  count   = 0;
    int   overlap = 0;

    while (1) {
        size_t n = fread(buf + overlap, 1, READ_CHUNK, fp);
        if (n == 0) break;

        int total  = overlap + (int)n;
        buf[total] = '\0';

        /* Scan for all non-overlapping occurrences */
        const char *p   = buf;
        const char *lim = buf + total - kw_len + 1;
        while (p < lim) {
            p = strstr(p, keyword);
            if (!p || p >= lim) break;
            count++;
            p += kw_len;
        }

        /* Carry tail into next iteration to handle boundary matches */
        overlap = (total >= kw_len - 1) ? kw_len - 1 : total;
        memmove(buf, buf + total - overlap, overlap);
    }

    free(buf);
    fclose(fp);
    return count;
}

/* ──────────────────────────────────────────────────────────────────────────
 * thread_worker()
 * Loops: claim file → search → write result → repeat until queue empty.
 * ────────────────────────────────────────────────────────────────────────── */
static void *thread_worker(void *arg)
{
    thread_args_t  *a = (thread_args_t *)arg;
    work_queue_t   *q = a->queue;
    output_state_t *o = a->out;

    while (1) {
        /* ── Claim next file ─────────────────────────────────────────── */
        pthread_mutex_lock(&q->queue_mutex);
        int idx = q->next_index;
        if (idx >= q->num_files) {
            pthread_mutex_unlock(&q->queue_mutex);
            break;
        }
        q->next_index++;
        pthread_mutex_unlock(&q->queue_mutex);

        const char *path = q->files[idx];

        /* ── Count (no lock — purely local work) ─────────────────────── */
        long hits = count_keyword_in_file(path, a->keyword, a->kw_len);

        /* ── Write result to shared output file ──────────────────────── */
        pthread_mutex_lock(&o->write_mutex);
        if (hits >= 0) {
            fprintf(o->fp, "[Thread %2d] %-40s => %ld occurrence(s)\n",
                    a->thread_id, path, hits);
            o->total_hits += hits;
        } else {
            fprintf(o->fp, "[Thread %2d] %-40s => ERROR opening file\n",
                    a->thread_id, path);
        }
        fflush(o->fp);
        pthread_mutex_unlock(&o->write_mutex);

        printf("  [Thread %2d] %-38s %ld hit(s)\n",
               a->thread_id, path, hits >= 0 ? hits : 0L);
    }

    pthread_exit(NULL);
}

/* ──────────────────────────────────────────────────────────────────────────
 * run_search()  — spins up threads, returns elapsed seconds
 * ────────────────────────────────────────────────────────────────────────── */
static double run_search(const char  *keyword,
                         const char **files,
                         int          num_files,
                         int          num_threads,
                         FILE        *out_fp,
                         long        *total_out)
{
    work_queue_t queue = {
        .files      = files,
        .num_files  = num_files,
        .next_index = 0,
    };
    pthread_mutex_init(&queue.queue_mutex, NULL);

    output_state_t out_state = { .fp = out_fp, .total_hits = 0 };
    pthread_mutex_init(&out_state.write_mutex, NULL);

    pthread_t     *threads = malloc(num_threads * sizeof(pthread_t));
    thread_args_t *args    = malloc(num_threads * sizeof(thread_args_t));
    if (!threads || !args) { perror("malloc"); exit(EXIT_FAILURE); }

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    for (int i = 0; i < num_threads; i++) {
        args[i] = (thread_args_t){
            .thread_id = i + 1,
            .keyword   = keyword,
            .kw_len    = (int)strlen(keyword),
            .queue     = &queue,
            .out       = &out_state,
        };
        if (pthread_create(&threads[i], NULL, thread_worker, &args[i]) != 0) {
            perror("pthread_create"); exit(EXIT_FAILURE);
        }
    }

    for (int i = 0; i < num_threads; i++)
        pthread_join(threads[i], NULL);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed = (t1.tv_sec  - t0.tv_sec) +
                     (t1.tv_nsec - t0.tv_nsec) / 1e9;

    *total_out = out_state.total_hits;

    pthread_mutex_destroy(&queue.queue_mutex);
    pthread_mutex_destroy(&out_state.write_mutex);
    free(threads);
    free(args);
    return elapsed;
}

/* ── main ────────────────────────────────────────────────────────────────── */
int main(int argc, char *argv[])
{
    /* ./search <keyword> <output.txt> <file1> [file2 ...] <num_threads> */
    if (argc < 5) {
        fprintf(stderr,
            "Usage: %s <keyword> <output.txt> <file1> [file2 ...] <num_threads>\n",
            argv[0]);
        exit(EXIT_FAILURE);
    }

    const char  *keyword     = argv[1];
    const char  *output_path = argv[2];
    int          num_threads = atoi(argv[argc - 1]);
    int          num_files   = argc - 4;          /* between output & last arg */
    const char **files       = (const char **)&argv[3];

    if (num_threads < 1) {
        fprintf(stderr, "Error: num_threads must be >= 1\n");
        exit(EXIT_FAILURE);
    }
    if (num_files < 1) {
        fprintf(stderr, "Error: provide at least one input file\n");
        exit(EXIT_FAILURE);
    }

    /* Cap threads at file count — idle threads add no value */
    if (num_threads > num_files) {
        printf("Note: capping threads from %d to %d (number of files)\n",
               num_threads, num_files);
        num_threads = num_files;
    }

    int cpu_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);

    /* ── Open output file ───────────────────────────────────────────────── */
    FILE *out_fp = fopen(output_path, "w");
    if (!out_fp) { perror("fopen output"); exit(EXIT_FAILURE); }

    fprintf(out_fp,
        "========================================\n"
        " Keyword Search Results\n"
        " Keyword  : \"%s\"\n"
        " Files    : %d\n"
        " Threads  : %d\n"
        " CPU cores: %d\n"
        "========================================\n\n",
        keyword, num_files, num_threads, cpu_cores);

    /* ── Run ────────────────────────────────────────────────────────────── */
    printf("\nSearching \"%s\" in %d file(s) using %d thread(s)...\n\n",
           keyword, num_files, num_threads);

    long   total_hits;
    double elapsed = run_search(keyword, files, num_files,
                                num_threads, out_fp, &total_hits);

    /* ── Summary to output file ─────────────────────────────────────────── */
    fprintf(out_fp,
        "\n========================================\n"
        " SUMMARY\n"
        " Total occurrences : %ld\n"
        " Threads used      : %d\n"
        " Elapsed time      : %.4f seconds\n"
        "========================================\n",
        total_hits, num_threads, elapsed);
    fclose(out_fp);

    /* ── Console summary ────────────────────────────────────────────────── */
    printf("\n========================================\n");
    printf("  Keyword        : \"%s\"\n",   keyword);
    printf("  Files searched : %d\n",       num_files);
    printf("  Threads used   : %d\n",       num_threads);
    printf("  CPU cores      : %d\n",       cpu_cores);
    printf("  Total hits     : %ld\n",      total_hits);
    printf("  Elapsed time   : %.4f s\n",   elapsed);
    printf("  Results saved  : %s\n",       output_path);
    printf("========================================\n\n");

    return 0;
}

