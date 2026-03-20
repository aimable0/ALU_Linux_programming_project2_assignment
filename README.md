# ALU Linux Programming — Project 2 Assignment

A collection of four C programs demonstrating core Linux systems programming concepts:
process management, I/O performance, multithreading, and concurrent file processing.
Each program is traced and analysed using `strace`.

---

## Repository Structure

```
.
├── project1/          # Q1 — Process pipeline (fork + execvp + pipe)
│   ├── pipeline.c
│   └── output.txt
│   └── strace_full.txt
├── project2/          # Q2 — File copy utility comparison
│   ├── copy_syscall.c
│   ├── copy_stdio.c
│   ├── run_test.sh
│   └── testfile.bin
├── project3/          # Q3 — Multi-threaded prime counter
│   └── prime_threads.c
├── project4/          # Q4 — Multi-threaded keyword search
│   ├── search.c
│   ├── run_search.sh
│   ├── README.md
│   ├── output_2t.txt
│   ├── output_4t.txt
│   ├── output_16t.txt
│   └── tests/
│       ├── file1.txt
│       └── ... file16.txt
└── README.md
```

---

## Question 1 — Process Pipeline (`project1/`)

Implements the shell pipeline `ps aux | grep root` using raw POSIX primitives.

**Concepts:** `fork()`, `execvp()`, `pipe()`, `dup2()`, `waitpid()`

```bash
gcc -Wall -o pipeline pipeline.c
./pipeline
```

Traced with `strace` to analyse the full sequence of system calls across parent and child processes.

```bash
strace -f -e trace=process,desc -o strace_out.txt ./pipeline
```

---

## Question 2 — File Copy Utility (`project2/`)

Two versions of a 100 MB file copy utility benchmarked against each other.

| Version | Method | File |
|---------|--------|------|
| 1 | Raw `read()` / `write()` syscalls | `copy_syscall.c` |
| 2 | Standard I/O `fread()` / `fwrite()` | `copy_stdio.c` |

```bash
gcc -Wall -o copy_syscall copy_syscall.c
gcc -Wall -o copy_stdio  copy_stdio.c

./copy_syscall source.bin dest.bin
./copy_stdio   source.bin dest.bin
```

Run the full comparison (generates test file, traces with `strace -c`, diffs output):
```bash
bash run_test.sh
```

**Key finding:** at equal buffer sizes (64 KB) both versions make identical syscall counts.
The stdio advantage appears only with small application-level read sizes.

---

## Question 3 — Multi-threaded Prime Counter (`project3/`)

Counts all prime numbers between 1 and 200,000 using 16 threads with `pthread_mutex_t`.

**Concepts:** `pthread_create`, `pthread_join`, `pthread_mutex_lock/unlock`, workload segmentation

```bash
gcc -Wall -pthread -o prime_threads prime_threads.c -lm
./prime_threads
```

**Expected output:**
```
The synchronized total number of prime numbers between 1 and 200000 is 17984
```

Each thread processes an equal segment of 12,500 numbers, counts locally, then adds to the shared counter under a mutex in a single lock/unlock — minimising contention to 16 lock cycles total.

---

## Question 4 — Multi-threaded Keyword Search (`project4/`)

Searches for a keyword across 16 text files concurrently, writing results to a shared output file.

**Concepts:** `fopen/fread`, `pthread_mutex_t`, dynamic work queue, performance benchmarking

```bash
gcc -Wall -pthread -o search search.c
```

Run all three required performance configurations automatically:
```bash
bash run_search_benchmark.sh
```

This runs:
| Configuration | Threads |
|---|---|
| Minimum | 2 |
| Average CPU cores | `nproc` (auto-detected) |
| Maximum | 16 (one per file) |

**Execution format:**
```bash
./search <keyword> <output.txt> <files...> <num_threads>

# Example:
./search the output.txt tests/*.txt 16
```

**Expected output file structure:**
```
=======================================================
 Keyword Search Results
 Keyword    : "the"
 Files      : 16
 Threads    : 16
=======================================================
File: tests/file1.txt  | Keyword: "the"  | Occurrences: 500
...
=======================================================
 TOTAL occurrences of "the" across all files : 5000
 Threads used   : 16
 Elapsed time   : X.XXX ms
=======================================================
```

> Note: file order in results varies between runs — expected behaviour due to thread concurrency.

---

## Requirements

- GCC with POSIX thread support
- Linux (tested on Ubuntu)
- `strace` for system call tracing (`sudo apt install strace`)

---

