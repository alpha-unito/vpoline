# Call for Artifacts - Middleware 2026

## Table of contents
- [Artifact description](#artifact-description)
  - [Badge requests](#badge-requests)
  - [System requirements](#system-requirements)
  - [Output interpretation](#output-interpretation)
  - [Artifact Content](#artifact-content)
- [QEMU](#qemu)
- [RISC-V Hardware](#risc-v-hardware)
- [Evaluating results](#evaluating-results)

## Artifact description
The artifact presented in this document allows to reproduce the tests and
benchmarks presented in section 5 of the paper "Exhaustive System Call
Interception on RISC-V". Scripts are provided to automatize the full process as
much as possible. Two options are provided:
- Run the test suite inside a [RISC-V virtual environment emulated by
Qemu](#qemu).
- Run the test suite on [actual RISC-V hardware](#risc-v-hardware).

Access to a machine supporting all the tested interception technologies can be
provided if the reviewers think that's necessary. Please contact the authors of
the paper in this case.

### Badge requests
We are requesting for all three possible badges as we provide an artifact to
reproduce the main results presented in the paper

### System requirements
- Debian-based Linux distribution running on RISC-V hardware to run the
  benchmarks natively, **or**
- Debian-based Linux distribution running on x86_64 hardware to run the
  benchmarks inside a QEMU virtual environment emulating a RISC-V machine.

We provide scripts and detailed step-by-step instructions to support reviewers
whichever option they choose. The scripts are designed to be run on a
Debian-based Linux distribution. `sudo` access can be required to install 
dependencies and load kernel modules.

### Output interpretation
Obtaining the same exact results as reported in the paper is very unlikely, as
different machines and kernel versions can lead to different results. Moreover,
some methods such as kprobes and SUD are not supported by default, so
guaranteeing that all interception methods will be available on possible
reviewers' machines is out of our control. That is why we provide instruction
for a virtual and tested environment. Time measurements on an emulation
will not be directly comparable with the ones obtained on actual hardware, but
they are still meaningful to compare the different interception methods and
evaluate how they perform against each other.

At the end of the experiment, majority of results will be grouped in dedicated
folders inside of `~/mw26_artifact_evaluation`, both that you opted for QEMU or
actual RISC-V hardware. The results for the redis benchmark will be stored in
the host machine instead, in the same directory where you ran
`run_redis_bench.sh`.
We suggest copying to your host machine the full `~/mw26_artifact_evaluation`,
creating a subdirectory `redis/` and grouping the redis-related CSVs there.
The provided python scripts can be downloaded from Zenodo and placed in the
related subdirectory to generate graphs.

### Artifact Content

- `capio-vpoline-main.zip`: CAPIO source code using vpoline as intercepting
   library. If you choose the QEMU option, you can ignore this .zip.
- `setup_qemu_environment.sh`: script to install all dependencies needed to run
   the QEMU virtual environment.
- `start_qemu.sh`: script to start the QEMU virtual environment.
- `setup_hardware_environment.sh`: script to install all dependencies needed to
   run the benchmarks on actual RISC-V hardware.
- `run_redis_bench.sh`: script to run the redis benchmark on the host machine.
- `ubuntu-riscv64-vpoline-artifact.qcow2`: QEMU image of Ubuntu 24.04 RISC-V
   with all dependencies and scripts pre-installed. If you choose the RISC-V
   hardware option, you can ignore this .qcow2 image.
- `<test_name>_plot.py`: python scripts to generate the graph for the related
   test. They have to be manually downloaded from Zenodo and placed

The majority of the scripts are not contained in the artifact as they are
contained in the `vpoline` repository.

The full `vpoline` source code is publicly available at the linked GitHub
repository:
https://github.com/alpha-unito/vpoline/tree/dev

## QEMU

After downloading the artifact from Zenodo, you'll need to run the following
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
./syscall_latency_bench.sh
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
replacing \<PID> with the PID copied from the first window:
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
./gp_fault_handling_benc.sh
```
**4. Macrobenchmark**

From `~/vpoline/artifact` run the following command:
```shell
./macrobenchmark.sh
```
Please note that we reduced the number of iterations to 5 for each interception
method just to present a minimal stats set and a proof of correct execution.
The results discussed in the paper were obtained out of 1000 iterations, which
would take several hours on actual RISC-V hardware.

**5. CAPIO**

Unfortunately, we did not manage to successfully run CAPIO in QEMU with vpoline
preloaded. For executing this benchmark we suggest using actual RISC-V
hardware, where this is possible without any restriction.

[//]: # (As for all other experiments, CAPIO and the benchmark executable are already)

[//]: # (compiled. All you need to do is from `~/vpoline/artifact` run the following)

[//]: # (command:)

[//]: # (```shell)

[//]: # (./capio_launch_bench.sh)

[//]: # (```)

**6. Redis**

To benchmark redis, we need to run the server and the client on two different
hosts. In this case, we suggest using the emulated RISC-V environment for the
server and the host machine (x86_64 or aarch64) for the client.\
The test must be run four times, one for each of the reported interception
methods.\
In the emulated RISC-V environment, execute the following command to natively
run redis-server:
```shell
#on emulated RISC-V environment
redis-server --bind 0.0.0.0 --protected-mode no
```
Then, on the host machine, assuming the `pwd` is `~/vpoline/artifact`, run the
following command to execute the redis benchmark:
```shell
#on host machine
./run_redis_bench.sh baseline.csv 127.0.0.1 6380
```
Wait for the host machine terminated, then `Ctrl+C` the redis-server on the
emulated RISC-V environment and repeat the process for the other interception
methods.

Vpoline
```shell
#on emulated RISC-V environment
LD_PRELOAD=../build/libvpoline.so \
LIBVPHOOK=../test/hook_forward.so \
redis-server --bind 0.0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh vpoline.csv 127.0.0.1 6380
```
Syscall_intercept
```shell
#on emulated RISC-V environment
LD_LIBRARY_PATH=../test:../../syscall_intercept/build \
LD_PRELOAD=../test/intercept_sys_forward.so \
redis-server --bind 0.0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh syscall_intercept.csv 127.0.0.1 6380
```
Strace
```shell
#on emulated RISC-V environment
strace -o /dev/null redis-server --bind 0.0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh strace.csv 127.0.0.1 6380
```

At this point all the results should be stored in `~/mw26_artifact_evaluation`.
We suggest to copy the full folder to your local machine to generate graphs.
Since results for redis are already in the host machine, we suggest moving them
with the other results with
```shell
mkdir -p ~/mw26_artifact_evaluation/redis
mv ~/vpoline/artifact/*.csv ~/mw26_artifact_evaluation/redis
```
Then you can run `sudo poweroff` to 
turn off the QEMU emulation and after a few seconds the first terminal that ran
the `start_qemu.sh` script will return to the command line.

In the downloaded artifact from Zenodo, we provide python scripts to generate
the graph which can be compared with the ones presented in the paper.


## RISC-V Hardware

**Disclaimer**: as mentioned, we provide scripts supporting Debian-based Linux
distributions. Moreover, support for SUD and kprobes is out of our control and
depends on the hardware/kernel configuration.

In case the artifact reviewers would like to run the benchmarks on actual RISC-V
hardware, then they will need to download the artifact from Zenodo and install
the needed dependencies. As mentioned before, we provide scripts that assume to
be executed on a Debian-based Linux distribution.\
Kprobes and SUD support depend on the hardware/kernel configuration. We cannot
guarantee that results all the interception methods will be available on your
machines.

First of all, make sure you're in your $HOME directory (`cd ~`) and download the
script `setup_hardware_environment.sh` from Zenodo
```shell
wget <replace-with-url-to-setup_hardware_environment.sh-from-zenodo>
```
Then run the script to install all dependencies:
```shell
./setup_hardware_environment.sh
```
Change working directory with `cd $HOME/vpoline/artifact`. Here all the scripts
are contained. Now you can start running the experiments.

**1. Patching coverage**

First you need to download and compile some different versions of the glibc.
This will likely take some time.
```shell
./download_glibc.sh 2.37
./download_glibc.sh 2.39
./download_glibc.sh 2.41
./download_glibc.sh 2.43
```
After all glibc are built, you can run the patching coverage test:
```shell
./run_patching_coverage.sh
```
**2. Syscall latency benchmark**

For this benchmark you need to follow the same exact steps as for the QEMU
environment. Run:
```shell
./syscall_latency_benchmark.sh
```

Now to measure kprobes latency, keep `$HOME/vpoline/artifact` as `pwd` and run:
```shell
../benchmark/syscall_latency ~/mw26_artifact_evaluation/syscall_latency/kprobes_syscall_latency.json 1
```
It will print a PID on screen. Copy it and in a new terminal window and from
your $HOME directory run the following commands to load the kernel module. You
need to replace \<PID> with the PID copied from the first window:
```shell
cd vpoline/benchmark/kprobes/build
sudo insmod example.ko target_pid=<PID>
```
You can now press `Enter` on the other terminal where the test is waiting after
the PID was printed. Remember to unload the kernel module with `sudo rmmod
example` after the test is completed.

**Disclaimer:** please remember that **kprobes** and **SUD** support depend on the
machine kernel. We cannot guarantee that results for all the interception
methods will be available.

**3. GP-fault handling benchmark**

For this benchmark, you just need to run:
```shell
./gp_fault_handling_bench.sh
```

**4. Macrobenchmark**

From `~/vpoline/artifact` run the following command:
```shell
./macrobenchmark.sh
```
Please note that we reduced the number of iterations to 5 for each interception
method just to present a minimal stats set and a proof of correct execution.
The results discussed in the paper were obtained out of 1000 iterations, which
would take several hours on actual RISC-V hardware.

**5. CAPIO**

Assuming `pwd` is `$HOME`, first of all we need to download CAPIO source code:
```shell
wget <replace-with-url-to-capio-vpoline-main-zip-from-zenodo>
unzip capio-vpoline-main.zip
```

Then, we need to compile CAPIO. From `~/vpoline/artifact` run:
```shell
./compile_capio.sh
```
Then you can run the benchmark with:
```shell
./capio_launch_bench.sh
```

**6. Redis**

To benchmark redis, we need to run the server and the client on two different
hosts. In this case, the server must run on the RISC-V machine while the host
machine (x86_64 or aarch64) will run the client which will generate reports.
The test must be run four times, one for each of the reported interception
methods.
We need to compile the hooks forwarding the system call to the kernel:
```shell
./compile_redis_hooks.sh
```
When launching the benchmarking script on the host machine, you need to replace
\<ip-address-of-riscv-machine> with the actual IP address of the RISC-V machine.
You can find it by running `ip a` on the RISC-V machine.

On the RISC-V machine, execute the following command to natively run
redis-server:
```shell
#on RISC-V machine
redis-server --bind 0.0.0.0 --protected-mode no
```
Then, on the host machine, wherever you downloaded artifact files, run:
```shell
#on host machine
./run_redis_bench.sh baseline.csv <ip-address-of-riscv-machine>
```
Wait for the host machine terminated, then `Ctrl+C` the redis-server on the
RISC-V machine and repeat the process for the other interception methods.

On the RISC-V machine you'll have to keep staying in `$HOME/vpoline/artifact`.

Vpoline
```shell
#on RISC-V machine
LD_PRELOAD=../build/libvpoline.so \
LIBVPHOOK=../test/hook_forward.so \
redis-server --bind 0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh vpoline.csv <ip-address-of-riscv-machine>
```
Syscall_intercept
```shell
#on RISC-V machine
LD_LIBRARY_PATH=../test:../../syscall_intercept/build \
LD_PRELOAD=../test/intercept_sys_forward.so \
redis-server --bind 0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh syscall_intercept.csv <ip-address-of-riscv-machine>
```
Strace
```shell
#on RISC-V machine
strace -o /dev/null redis-server --bind 0.0.0 --protected-mode no
```
```shell
#on host machine
./run_redis_bench.sh strace.csv <ip-address-of-riscv-machine>
```

## Evaluating results

After executing all the experiments, results will be store in the
`~/mw26_artifact_evaluation` directory, except the redis results files which are
now stored wherever you executed `run_redis_bench.sh`. As mentioned before, we
suggest copying the full folder to your host machine, creating a subdirectory
called `redis/` and moving the redis-related CSVs there.

Experiment 1 does not require any graph generation. Results can be evaluated
by inspecting the generated reports and comparing them with results presented in
the paper.

Experiments 2-6: the Zenodo repository contains python scripts to generate
graphs to be compared with the ones presented in the paper. They have to be
manually downloaded and placed in the related subdirectory of
`~/mw26_artifact_evaluation`. Executing them will generate the related graph in
as a PDF file.