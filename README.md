# vpoline: heuristics-free system call interception on RISC-V

vpoline (RISC-**V** tram**poline**) is the first system call interception library
for RISC-V based on binary rewriting while being completely free from any heuristics.
It combines the high speed of binary rewriting and the exhaustiveness of kernel
interfaces.

[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

# Dependencies
## Runtime dependencies
- libcapstone >= 5 -- the disassembly engine used under the hood. This is
  **automatically** fetched and compiled when you build vpoline.
## Build dependencies
- cmake >= 3.14

# How to build
You can create a build directory and build the project using cmake as follows.
libvpoline.so wil be generated in the build directory.
```
mkdir build
cd build
cmake ..
make
```
Running test suite (currently under development)
```
make test
```

# Synopsis
vpoline is a novel system call interception library for the RISC-V ecosystem.\
While other binary rewriting approaches were already available on RISC-V, they
rely on heuristic algorithms to find suitable space in the target executable
where to overwrite `ecall` instruction occurrences and their surroundings with
the needed machine language instructions to perform the interception (typically
with a jump to the code actually handling the interposition). While this kind of
solutions could work in many cases, it is not guaranteed to work in 100% of the
cases and is prone to failure since assumptions about the target binary could be
violated by future compiler optimizations or newer libraries versions. E.g.,
some assumptions could work for glibc 2.37 but not for glibc 2.41, resulting in
an unreliable interception mechanism.\
At the same time this issue was solved in the x86_64 ecosystem by the
introduction of [zpoline](https://github.com/yasukata/zpoline) which finds in
the 2-bytes long `syscall` enough space for overwriting a jump to a `nop`-slide
encoded at address 0x0.\
However this is not possible to port on RISC-V because of several major ISA
differences. Moreover, allocating space at address 0x0 requires root privileges
which are not granted to HPC clusters users. Since binary rewriting based
interception is often preferred for HPC applications, such requirement would be
a major limitation for its adoption in the HPC ecosystem.\
vpoline combines the total absence of heuristics without requiring root
privileges.

# How to use
Assuming libvpoline.so is already built, we need to compile a hook library, i.e.,
a shared library the user should implement to define what the actual hook
function will do. As an example, we provide a template hook library at
[test/hook_template.c](test/hook_template.c). It does not do anything more than
printing the identifying number of the system call being intercepted.\
Assuming to be within the `test/` dir
```
gcc -o hook_template.so hook_template.c -I../include -fpic -shared
```
Then we can see which system calls are being issued by a simple program like `ls`
by running (still from within `test/`)
```
LD_PRELOAD=../build/libvpoline.so LIBVPHOOK=./hook_template.so ls
```
And it should return a similar output
```
output from __hook_init: we can do some init work here
output from hook_function: syscall number 56
output from hook_function: syscall number 80
output from hook_function: syscall number 222
output from hook_function: syscall number 57
output from hook_function: syscall number 56
output from hook_function: syscall number 80
output from hook_function: syscall number 63
output from hook_function: syscall number 63
output from hook_function: syscall number 57
output from hook_function: syscall number 56
output from hook_function: syscall number 56
output from hook_function: syscall number 56
output from hook_function: syscall number 56
output from hook_function: syscall number 56
output from hook_function: syscall number 56
output from hook_function: syscall number 29
output from hook_function: syscall number 29
output from hook_function: syscall number 56
output from hook_function: syscall number 80
output from hook_function: syscall number 61
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 291
output from hook_function: syscall number 61
output from hook_function: syscall number 57
output from hook_function: syscall number 29
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 64
CMakeLists.txt  LICENSE  LICENSE-zpoline  Makefile  README.md  benchmark  build  capstone  include  src  steps.md  test
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 134
output from hook_function: syscall number 57
```

### Interface
To execute a system call without it being intercepted, the user can call
`syscall_no_intercept()` from within the hook function. The pointer to this
function is passed as third argument to `__hook_init()` and has to be assigned
to a `syscall_no_intercept_t` function pointer which can be renamed as you
prefer.\
In the hook function we can picture two possible scenarios:
- **Forward**: the user wants the system call to be regularly executed after the
    hook function returns. In this case `hook()` **must** return 1.
- **Handle**: the user handles the system call within the hook function (be it
    by emulating it or by calling `syscall_no_intercept()`) and does not want it to
    be forwarded to the kernel. In this case `hook()` **must** return 0.

# Assumptions and limitations
- This library just works on Linux on RISC-V 64-bit CPUs.
- Currently tested with glibc. Patching coverage is currently 100% across all
  the glibc versions we tested (2.35, 2.37, 2.39, 2.41, 2.43).

# Coming soon
- Introduction of post_clone hooks, both for parent and child thread.
- Fully developed test suite.
- Support for targeting any executable, not just glibc.

# Acknowledgements
This work is developed in the context of the DARE SGA1 project, which has
received funding from the European High-Performance Computing Joint Undertaking
(JU) under grant agreement No 101202459. The JU receives support from the
European Union’s Horizon Europe research and innovation programme and Spain,
Germany, Czechia, Italy, Netherlands, Belgium, Finland, Greece, Croatia,
Portugal, Poland, Sweden, France and Austria. Funded by the European Union. 

# vpoline team
- Ottavio Monticelli (Maintainer)
- Marco Edoardo Santimaria (Maintainer)
- Marco Aldinucci (Maintainer and Principal Investigator)
- Iacopo Colonnelli (Maintainer and Principal Investigator)
