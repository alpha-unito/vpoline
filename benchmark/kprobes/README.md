# Direct syscall kprobes

This x86-64 and RISC-V kernel module registers kprobes directly on `open`,
`close`, `read`, `write`, `stat`, and `clone`. On RISC-V, the native `openat`
and `newfstatat` symbols provide the `open` and `stat` events. Calls from the
selected process are written to the kernel log.

Build and start the test program:

```sh
make
./build/test
```

Load the module with the PID printed by the test:

```sh
sudo insmod build/example.ko target_pid=12345
```

Press Enter in the test terminal, inspect the messages, and unload the module:

```sh
sudo dmesg
sudo rmmod example
```
