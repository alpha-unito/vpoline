#!/bin/bash

echo "============================================================"
echo " vpoline Artifact Setup - QEMU RISC-V Environment"
echo "============================================================"


echo "[*] Step 1: Installing required packages (QEMU, wget, unzip, etc.)"
echo "    You might be prompted for your sudo password."
sudo apt-get update
sudo apt-get install -y qemu-system-misc qemu-utils wget curl unzip \
                        openssh-client opensbi u-boot-qemu

if ! command -v qemu-system-riscv64 &> /dev/null; then
    echo "[!] Error: qemu-system-riscv64 not found. Installation failed."
    exit 1
fi
echo "[+] QEMU for RISC-V installed successfully."
