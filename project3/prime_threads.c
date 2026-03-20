/*
 * prime_threads.c
 * Counts prime numbers between 1 and 200,000 using 16 threads.
 * Compile: gcc -Wall -pthread -o prime_threads prime_threads.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <pthread.h>

#define NUM_THREADS  16
#define UPPER_LIMIT  200000

/* ── Shared state ─────────────────────────────────────────────────────── */
static long           prime_count = 0;
static pthread_mutex_t count_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ── Per-thread work descriptor ───────────────────────────────────────── */
typedef struct {
    int thread_id;
    long range_start;   /* inclusive */
    long range_end;     /* inclusive */
} thread_args_t;

/* ── Primality test (trial division) ─────────────────────────────────── */
static int is_prime(long n) {
    if (n < 2)  return 0;
    if (n == 2) return 1;
    if (n % 2 == 0) return 0;
    long limit = (long)sqrt((double)n);
    for (long i = 3; i <= limit; i += 2)
        if (n % i == 0) return 0;
    return 1;
}

/* ── Thread worker ────────────────────────────────────────────────────── */
static void *count_primes_in_segment(void *arg) {
    thread_args_t *args  = (thread_args_t *)arg;
    long local_count = 0;

    /* Count primes locally — no lock needed here */
    for (long n = args->range_start; n <= args->range_end; n++)
        if (is_prime(n))
            local_count++;

    /* Add local result to shared counter under mutex */
    pthread_mutex_lock(&count_mutex);
    prime_count += local_count;
    pthread_mutex_unlock(&count_mutex);


    pthread_exit(NULL);
}

/* ── Main ─────────────────────────────────────────────────────────────── */
int main(void) {
    pthread_t     threads[NUM_THREADS];
    thread_args_t args[NUM_THREADS];

    long segment = UPPER_LIMIT / NUM_THREADS;   /* 200000 / 16 = 12500 */


    /* ── Create threads ──────────────────────────────────────────────── */
    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].thread_id   = i + 1;
        args[i].range_start = i * segment + 1;
        /* Last thread takes any remainder from integer division */
        args[i].range_end   = (i == NUM_THREADS - 1)
                              ? UPPER_LIMIT
                              : (i + 1) * segment;

        if (pthread_create(&threads[i], NULL,
                           count_primes_in_segment, &args[i]) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }

    /* ── Join all threads ────────────────────────────────────────────── */
    for (int i = 0; i < NUM_THREADS; i++) {
        if (pthread_join(threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(EXIT_FAILURE);
        }
    }

    pthread_mutex_destroy(&count_mutex);

    /* ── Final result ────────────────────────────────────────────────── */
    printf("\nThe synchronized total number of prime numbers"
           " between 1 and %d is %ld\n",
           UPPER_LIMIT, prime_count);

    return 0;
}
