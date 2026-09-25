#!/bin/bash

IMAGE_FILE=ubuntu-riscv64-vpoline-artifact.qcow2
SSH_PORT=2222

echo "=================================================================="
echo "[*] Starting RISC-V VM in interactive console mode..."
echo "[*] Please wait for the boot process to complete (it may take 1-2 minutes)."
echo "[*] QEMU EMERGENCY EXIT:"
echo "    To forcefully exit QEMU at any time: press Ctrl+a, release, then press x"
echo "=================================================================="

qemu-system-riscv64 \
    -machine virt \
    -m 4G \
    -smp 4 \
    -cpu rv64 \
    -nographic \
	-bios default \
    -kernel /usr/lib/u-boot/qemu-riscv64_smode/uboot.elf \
    -drive file=$IMAGE_FILE,format=qcow2,if=virtio \
	-netdev user,id=net0,hostfwd=tcp::$SSH_PORT-:22 \
	-device virtio-net-device,netdev=net0 \
	> qemu_output.log 2>&1 &

QEMU_PID=$!

trap "echo -e '\n[*] Terminal closed or Ctrl+C pressed. Shutting down QEMU...'; kill $QEMU_PID 2>/dev/null; exit" SIGINT SIGTERM SIGHUP

echo "[+] QEMU is running (PID: $QEMU_PID)."
echo "[*] Please wait 1-2 minutes for the VM to complete the boot sequence."
echo "[*] Afterwards, open a NEW terminal window and connect via SSH:"
echo ""
echo "    ssh -p $SSH_PORT ubuntu@localhost"
echo "    (Password: middleware)"
echo ""
echo "[!] DO NOT close this terminal! It will remain active while QEMU runs."
echo "[*] To gracefully shut down the VM, run 'sudo poweroff' from within the SSH session."
echo "[*] To forcefully kill it, press Ctrl+C here."
echo ""
echo "[*] Waiting for QEMU to power off..."

wait $QEMU_PID

echo "[+] QEMU has successfully powered off. Goodbye!"
