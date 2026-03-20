# Version 1
./copy_syscall testfile.bin dst_syscall.bin

# Version 2
./copy_stdio testfile.bin dst_stdio.bin

# With strace
strace -c ./copy_syscall testfile.bin dst_syscall.bin
strace -c ./copy_stdio  testfile.bin dst_stdio.bin
