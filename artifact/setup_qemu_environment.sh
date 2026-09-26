#
# Copyright 2026 University of Turin
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

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
