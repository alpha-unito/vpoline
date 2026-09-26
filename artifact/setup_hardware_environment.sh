#!/bin/bash

echo "============================================================"
echo " vpoline Artifact Setup - Hardware RISC-V Environment"
echo "============================================================"

echo "[*] Step 1: Installing required packages (wget, unzip, etc.)"
echo "    You might be prompted for your sudo password."

sudo apt-get update
sudo apt-get install -y wget curl unzip libopenmpi-dev build-essential bison \
    redis patchelf