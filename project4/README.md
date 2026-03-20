# Question 4 — Multi-threaded Keyword Search

## Files
```
search.c                 # main program source
run_search.sh  # runs all 3 thread configurations automatically
tests/
  file1.txt  ... file16.txt   # 16 test text files (500 lines each)
```

## Compile
```bash
gcc -Wall -pthread -o search search.c
```

## Run
Use the provided `run_search.sh` in the same directory:
```bash
bash run_search.sh
```
Automatically detects your CPU core count and runs all 3 configurations.

## Expected Output (terminal)
```
Sammple of important sections
========================================
Keyword        : "the"
Files searched : 16
Threads used   : 16
CPU cores      : 6
Total hits     : 5000
Elapsed time   : 0.0117 s
Results saved  : output_16t.txt
========================================
```

## Output File Structure
Each output file (e.g. `output_2t.txt`) looks like:
```
=======================================================
Keyword Search Results
Keyword    : "the"
Files      : 16
Threads    : 2
CPU cores  : 6
=======================================================
[Thread  1] tests/file1.txt                          => 500 occurrence(s)
[Thread  2] tests/file10.txt                         => 500 occurrence(s)
...
=======================================================
SUMMARY
Total occurences: 5000
Threads used    : 2
Elapsed time    : 8.267 seconds
=======================================================
```

## Expected Total
Searching for keyword `the` across all 16 test files → **5000 occurrences**

> Note: file order in the output may vary between runs — this is normal and
> expected since threads process files concurrently and finish at different times.
> Elapsed time may vary from per system.

