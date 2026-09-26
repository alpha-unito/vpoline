# Call for Artifacts - Middleware 2026

## Table of Contents
- [Artifact Description](#artifact-description)
- [Artifact Available](#artifact-availability)
- [QEMU](#qemu)
- [RISC-V Hardware](#risc-v-hardware)

## Artifact Description
The artifact presented in this document allows to reproduce the tests and
benchmarks presented in section 5 of the paper "Exhaustive System Call
Interception on RISC-V". Scripts are provided to automatize the full process as
much as possible. Two options are provided:
- Run the test suite inside a [RISC-V virtual environment emulated by
Qemu](#qemu).
- Run the test suite on [actual RISC-V hardware](#risc-v-hardware).

### Requirements
- The provided scripts expect to be run on a **Debian-based** Linux distribution,
both that reviewers plan to run QEMU on an x86_64 or aarch64 host, or that
reviewers plan to run the benchmarks on actual RISC-V hardware.
- `sudo` privileges are required to install dependencies and to run some of the
benchmarks (e.g., kprobes).

## Artifact Availability
The full artifact is publicly available at the linked zenodo repository:
https://zenodo.org/record/10000000. Files for both options are contained.

The full `vpoline` source code is publicly available at the linked GitHub
repository:
https://github.com/alpha-unito/vpoline/tree/dev

## QEMU

After downloading the artifact from zenodo, you'll need to run the following
script to install all necessary dependencies.
```shell
./setup_qemu_environment.sh
```
Then you can boot the provided Ubuntu RISC-V image by running the
following script in the same directory where you placed the downloaded .qcow2
image file.:
```shell
./start_qemu.sh
```

To avoid window size issue within terminal, the terminal running the previous
script will just run the QEMU emulation without command line interaction.
To actually use the virtual environment you'll need to open a new terminal and
connect to the QEMU instance via SSH. The following command can be used to
connect to the virtual environment (a few minutes may be needed for the virtual
environment to boot and be ready to accept SSH connections):
```shell
ssh -p 2222 ubuntu@localhost
```
Use the following credentials to log in:
* User: `ubuntu`
* Password: `middleware`

At this point we assume your `pwd` is `/home/ubuntu`. All dependencies and
scripts to run the experiments are ready to use.

First of all, `vpoline` has to be compiled:
```shell
cd vpoline
mkdir build && cd build
cmake ..
make -j$(nproc)
```
Then move in the artifact directory
```shell
cd ../artifact
```
We can now run the 6 experiments described in the paper.\

**1. Patching coverage:**
```shell
./run_patching_coverage.sh
```

**2. Syscall latency benchmark:**
```shell
./run_syscall_latency_benchmark.sh
```
Now results for all methods but kprobes are recorded. Since measuring kprobes
cannot be fully automated, an addition terminal window will be needed (or using
`tmux` to split the screen works fine as well).\
In the first window, run this command to start the kprobes measurement:
```shell
../benchmark/syscall_latency ~/mw26_artifact_evaluation/syscall_latency/kprobes_syscall_latency.json 1
```
It will print PID on screen, copy it and use it in the second windows to
correctly load the kernel module. Assuming that after connecting via SSH `pwd`
is `/home/ubuntu/` in the additional window, run the following command by
replacing <PID> with the PID copied from the first window:
```shell
cd vpoline/benchmark/kprobes/build
sudo insmod example.ko target_pid=<PID>
```
After that just press `Enter` in the first window to start the measurement.\
Once the measurement is completed, remember to unload the kernel module by
running `sudo rmmod example` (it's not relevant on which terminal). You can run
`exit` on the second terminal to close it.

**3. GP-fault handling benchmark:**

Let's make sure we're in `~/vpoline/artifact` before running the following
command:
```shell
./gp_fault_handling_benchmark.sh
```
**4. Macrobenchmark**

From `~/vpoline/artifact` run the following command:
```shell
./macrobenchmark.sh
```
Please note that we reduced the number of iterations to 5 for each interception
method just to present a minimal stats set and a proof of correct execution.
The results discussed in the paper were obtained out of 1000 iterations, which
would take several hours to actual RISC-V hardware.

**5. CAPIO**

As for all other experiments, CAPIO and the benchmark executable are already
compiled. All you need to do is from `~/vpoline/artifact` run the following
command:
```shell
./capio_launch_bench.sh
```


**6. redis**

To benchmark redis, we need to run the server and the client on two different
hosts. In this case, we suggest using the emulated RISC-V environment for the
server and the host machine (x86_64 or aarch64) for the client.\

When finished, you can copy the results stored in `~/mw26_artifact_evaluation`
to your local machine to generate graphs. Then you can run `sudo poweroff` to 
turn off the QEMU emulation and after a few seconds, the first terminal that ran
the `start_qemu.sh` script will return to the command line.

## RISC-V Hardware
In case the artifact reviewers would like to run the benchmarks on actual RISC-V
hardware, then they will need download the artifact from zenodo and install the
needed dependencies. As mentioned before, we provide scripts that assume to be
executed on a Debian-based Linux distribution.\
In this case, kprobes and SUD support depend on the hardware/kernel
configuration. We cannot guarantee that results all the interception methods
will be available on your machines.

## Evaluating results

After executing all the experiments, results will be store in the
`~/mw26_artifact_evaluation` directory. We suggest copying the full folder to
your local machine to generate graphs.

**1. Patching coverage:**
This result can be evaluated by inspection of the reported results
