# Config 1: 2 threads
./search the output_2t.txt tests/*.txt 2

# Config 2: average CPU cores
./search the output_4t.txt tests/*.txt 6

# Config 3: maximum — 16 threads (1 per file)
./search the output_16t.txt tests/*.txt 16
