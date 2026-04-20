# 🌌 Project NYX: Unified Ring-0 WebAssembly Composable Unikernel

![Linux Kernel](https://img.shields.io/badge/Linux-Kernel_6.x-yellow?style=for-the-badge&logo=linux)
![C](https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c)
![WebAssembly](https://img.shields.io/badge/Runtime-WebAssembly-orange?style=for-the-badge&logo=webassembly)
![eBPF](https://img.shields.io/badge/Network-eBPF_XDP-red?style=for-the-badge)

Project Nyx is an experimental, bare-metal systems architecture that completely bypasses the traditional operating system privilege boundary (Ring-3 user-space vs. Ring-0 kernel-space) to achieve blazing-fast, mathematically sandboxed microservice execution. 

By embedding a WebAssembly (Wasm) virtual machine directly inside the Linux kernel, translating storage via the Virtual File System (VFS), intercepting raw network traffic at the silicon level with XDP, and locking down execution with Linux Security Modules (LSM), Project Nyx processes network payloads and disk I/O at native hardware speeds without a single user-space context switch.

---

## ⚠️ CRITICAL WARNING

**This project involves loading unsigned, highly privileged code directly into Ring-0 of the Linux Kernel.** A single memory violation in the C wrappers can result in an immediate, unrecoverable Kernel Panic. Do **not** run this architecture on a host machine, daily driver, or production server. Use an isolated Virtual Machine (e.g., VirtualBox, WSL2, or a dedicated cloud instance).

---

## 🏗️ The Architecture (The 4 Pillars)

Project Nyx fundamentally reimagines how cloud and edge-computing workloads are executed by merging four cutting-edge technologies:

### 1. The Engine: In-Kernel WebAssembly (Phase 1)
Traditionally, running user-provided code in the kernel is a catastrophic security risk. Nyx solves this by porting the lightweight `wasm3` runtime into a standalone Linux Kernel Module (`nyx_core.ko`). WebAssembly acts as a virtual Instruction Set Architecture (ISA) protected by strict mathematical memory boundaries. This allows untrusted user payloads to execute in Ring-0 at native speeds while remaining completely isolated from host memory.

### 2. The Translator: WALI VFS Integration (Phase 2)
The WebAssembly Linux Interface (WALI) is a custom abstraction layer that bridges the sandbox to the host SSD. It intercepts WebAssembly system requests (`fd_read`, `fd_write`) and maps them seamlessly to the Linux kernel's internal **Virtual File System (VFS)** using `filp_open` and `kernel_read`. This allows the sandboxed payload to physically read and write host files without triggering `copy_from_user` overhead.

### 3. The Steerer: XDP Silicon Networking (Phase 3)
Instead of relying on the heavy, slow Linux TCP/IP networking stack (Netfilter, sockets), Nyx attaches an **eBPF (Extended Berkeley Packet Filter)** program directly to the **eXpress Data Path (XDP)** inside the Network Interface Card (NIC) driver. It parses raw electrical network pulses before the kernel even allocates memory, filters for HTTP traffic, measures the payload, and writes the packet size into a shared Ring-0 memory map for the Wasm engine to consume natively.

### 4. The Cage: LSM Zero-Trust Sandbox (Phase 4)
Because the Wasm VM operates in Ring-0, an exploit in the compiler itself could compromise the machine. Nyx deploys a secondary eBPF program attached to the kernel's **Linux Security Module (LSM)** hooks (e.g., `file_open`). This acts as an external failsafe cage. If the Wasm background thread attempts an unauthorized file or network action, the LSM hook intercepts the syscall in microseconds and hard-denies it with a `-EPERM` (Permission Denied) error.

---

## 📂 Repository Structure

```text
NYX/
├── wasm-engine/          # Heavily modified Wasm3 runtime (libc stripped, kmalloc wired)
├── nyx_init.c            # The Ring-0 Kernel Module, kthread heartbeat, and WALI VFS bridge
├── nyx_net.bpf.c         # The eBPF XDP silicon-level network sensor
├── nyx_lsm.bpf.c         # The eBPF LSM Zero-Trust security cage
├── wali_test.wat         # Wasm payload proving VFS read/write capabilities
├── wasm_payload.h        # Auto-generated C-array of the compiled Wasm binary
├── Makefile              # Kernel module build instructions
└── README.md             # Project documentation
```

---

## 🚀 Ignition Sequence (How to Build & Run)

### Prerequisites
You need a full systems-engineering toolchain installed on your Linux machine, including BTF support for the LSM hooks:

```bash
sudo apt update
sudo apt install build-essential linux-headers-$(uname -r)
sudo apt install clang llvm libbpf-dev linux-tools-common linux-tools-generic linux-tools-$(uname -r)
sudo apt install wabt
```

### Step 1: Forge the WebAssembly Payload
Compile the raw `.wat` (WebAssembly Text) file into a binary, and convert it to a C-header array so the kernel module can absorb it:

```bash
wat2wasm wali_test.wat -o wali_test.wasm
xxd -i wali_test.wasm > wasm_payload.h
```

### Step 2: Build and Inject the Ring-0 Engine
Compile the Linux Kernel Module and inject it into the live kernel. This spawns the `nyx_engine_thread` background heartbeat:

```bash
make clean && make
sudo insmod nyx_core.ko
```

Verify the engine ignited and successfully breached the VFS (as programmed in `wali_test.wat`):

```bash
sudo dmesg | tail -n 8
cat /tmp/nyx_proof.txt
# Expected Output: VFS successfully breached from Ring-0 Wasm Sandbox!
```

### Step 3: Arm the XDP Network Steerer
Compile the eBPF network sensor into restricted bytecode:

```bash
clang -g -O2 -target bpf -I/usr/include/x86_64-linux-gnu -c nyx_net.bpf.c -o nyx_net.bpf.o
```

Find your network interface name (`ip link`) and attach the eBPF program directly to the hardware driver (replace `enp0s3` with your interface):

```bash
sudo ip link set dev enp0s3 xdp obj nyx_net.bpf.o sec xdp
```

### Step 4: Lock the LSM Security Cage
Extract the `vmlinux.h` matrix from your live kernel:

```bash
sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```

Compile the LSM hook and attach it to the kernel's security subsystem:

```bash
clang -g -O2 -target bpf -D__TARGET_ARCH_x86 -I. -c nyx_lsm.bpf.c -o nyx_lsm.bpf.o
sudo bpftool prog loadall nyx_lsm.bpf.o /sys/fs/bpf/nyx_lsm autoattach
```

### Step 5: Intercept the Matrix
Clear the kernel trace buffer, set up a real-time monitor, and fire an HTTP request.

**Terminal 1 (The Monitor):**
```bash
sudo sh -c 'echo > /sys/kernel/debug/tracing/trace'
sudo cat /sys/kernel/debug/tracing/trace_pipe | grep NYX-XDP
```

**Terminal 2 (The Trigger):**
```bash
wget -qO- [http://example.com](http://example.com) > /dev/null
```

**Result in Terminal 1:**
```plaintext
[NYX-XDP] Silicon Drop! HTTP Packet Size: 60 bytes
[NYX-XDP] Silicon Drop! HTTP Packet Size: 891 bytes
```

### Step 6: Graceful Teardown
To safely kill the Ring-0 background thread, free the Wasm memory, detach the NIC sensor, and remove the module:

```bash
sudo ip link set dev enp0s3 xdp off
sudo rmmod nyx_core
```

---

## 🔮 Future Roadmap

* **Project Zyo Integration:** Tie the Unikernel kthread directly into a custom, AI-driven eBPF CPU scheduler to guarantee zero-jitter priority processing for incoming network payloads over standard host processes.
* **Quantum RNG WALI Mapping:** Expand the WALI translation layer to securely inject high-entropy quantum random numbers (QRNG) directly into the WebAssembly sandbox memory space for post-quantum cryptographic workloads.
* **SMP Multi-Core Scaling:** Refactor the engine loop to scale across multiple CPU cores, spawning dedicated Wasm execution instances for parallel network packet processing.

---

## 👤 Author
TheSNMC