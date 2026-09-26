0775

echo "============================================================"
echo " vpoline Artifact Setup - Hardware RISC-V Environment"
echo "============================================================"

echo "[*] Step 1: Installing required packages (wget, unzip, etc.)"
echo "    You might be prompted for your sudo password."

sudo apt-get update
sudo apt-get install -y wget curl unzip libopenmpi-dev build-essential bison \
    redis patchelf

echo "[*] Step 2: Downloading vpoline, syscall_intercept, capio and capstone 6"
git clone https://github.com/alpha-unito/vpoline.git
cd vpoline && git checkout dev && cd ..
git clone https://github.com/GekkoFS/syscall_intercept.git
cd syscall_intercept && git checkout riscv && cd ..
wget https://github.com/capstone-engine/capstone/archive/refs/tags/6.0.0-Alpha4.zip

echo "[*] Step 3: Building capstone 6.0.0-Alpha4"
unzip 6.0.0-Alpha4.zip
cd capstone-6.0.0-Alpha4
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCAPSTONE_ARCHITECTURE_DEFAULT=0 -DCAPSTONE_RISCV_SUPPORT=1
cmake --build build
mkdir -p $HOME/capstone-install
cmake --install build --prefix $HOME/capstone-install

echo "[*] Step 4: Building syscall_intercept"
cp $HOME/vpoline/artifact/CMakeLists_syscall_intercept.txt $HOME/syscall_intercept/CMakeLists.txt
mkdir -p $HOME/syscall_intercept/build
cd $HOME/syscall_intercept/build
cmake .. && make
cd

echo "[*] Step 5: Building vpoline"
mkdir -p $HOME/vpoline/build
cd $HOME/vpoline/build
cmake .. && make
cd

