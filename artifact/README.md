# Call for Artifacts - Middleware 2026

## Table of Contents
- [Artifact Description](#artifact-description)
- [Artifact Available](#artifact-availability)
- [QEMU login](#qemu-login)
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
The provided scripts expect to be run on a **Debian-like** Linux distribution.

## Artifact Availability
The full artifact is publicly available at the linked zenodo repository:
https://zenodo.org/record/10000000. Files for both options are contained.

The full `vpoline` source code is publicly available at the linked GitHub
repository:
https://github.com/alpha-unito/vpoline/tree/dev

## QEMU
* User: `ubuntu`
* Password: `middleware`

## RISC-V Hardware
In case the artifact reviewers have access to one of more RISC-V machines, 

[//]: # (## Artifact Functional)

[//]: # (To demonstrate how the artifact can be successfully installed, executed, and)

[//]: # (used, we set up an Ubuntu RISC-V image where all the needed utilities are)

[//]: # (already installed, which can be easily boot inside Qemu.\)

[//]: # (Scripts to automatically set up the dependencies to run Qemu, download the)

[//]: # (image and boot it are provided. They require a Debian-like Linux distribution)

[//]: # (and can be executed on any x86_64 or aarch64 host.\)

[//]: # (All the tests and benchmarks described in the paper will be ready to execute)

[//]: # (within the virtual environment. While performance hierarchy stays constant,)

[//]: # (time measurements will not reflect those obtained on the three platforms tested)

[//]: # (in the paper.)

[//]: # ()
[//]: # ()
[//]: # (## Artifact Reproduced)

[//]: # (To get the same results presented in the paper, it will be necessary to run the)

[//]: # (benchmarks on the three tested platforms mentioned in the paper. If those)

[//]: # (Benchmarks results will be automatically generated in .json and .txt files.)

[//]: # (Graph generation scripts are provided, but you'll have to transfer the results)

[//]: # (files from the RISC-V machines to your local machine.)

