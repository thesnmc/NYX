# 🌌 Project NYX: Unified Ring-0 WebAssembly Composable Unikernel

![Linux Kernel](https://img.shields.io/badge/Linux-Kernel_6.x-yellow?style=for-the-badge&logo=linux)
![C](https://img.shields.io/badge/Language-C-blue?style=for-the-badge&logo=c)
![WebAssembly](https://img.shields.io/badge/Runtime-WebAssembly-orange?style=for-the-badge&logo=webassembly)
![eBPF](https://img.shields.io/badge/Network-eBPF-red?style=for-the-badge)

Project Nyx is an experimental, bare-metal systems architecture that completely bypasses the traditional operating system privilege boundary (Ring-3 user-space vs. Ring-0 kernel-space) to achieve blazing-fast, mathematically sandboxed microservice execution. 

By embedding a WebAssembly (Wasm) virtual machine directly inside the Linux kernel, intercepting raw network traffic with eBPF, and locking down execution with Linux Security Modules (LSM), Project Nyx processes network payloads at native hardware speeds without a single user-space context switch.

---

## ⚠️ CRITICAL WARNING
**This project involves loading unsigned, highly privileged code directly into Ring-0 of the Linux Kernel.** A single memory violation in the C wrappers can result in an immediate, unrecoverable Kernel Panic. Do **not** run this architecture on a host machine, daily driver, or production server. Use an isolated Virtual Machine (e.g., VirtualBox, WSL2, or a dedicated cloud instance).

---

## 🏗️ The Architecture (The 4 Pillars)

Project Nyx fundamentally reimagines how cloud and edge-computing workloads are executed by merging four cutting-edge technologies:

### 1. The Engine: In-Kernel WebAssembly (Phase 1)
Traditionally, running user-provided code in the kernel is a catastrophic security risk. Nyx solves this by porting the lightweight `wasm3` runtime into a standalone Linux Kernel Module (`nyx_core.ko`). WebAssembly acts as a virtual Instruction Set Architecture (ISA) protected by strict mathematical memory boundaries. This allows untrusted user payloads to execute in Ring-0 at native speeds while remaining completely isolated from host memory.

### 2. The Translator: WALI (Phase 2)
The WebAssembly Linux Interface (WALI) is a custom abstraction layer. It intercepts WebAssembly System Interface (WASI) requests from the `.wasm` binary and maps them seamlessly to internal Linux kernel functions. For example, a Wasm `print` command is caught by WALI and translated directly to the kernel's `printk` function, entirely bypassing the `copy_from_user` overhead of standard syscalls.

### 3. The Steerer: eBPF TCX Zero-Copy Networking (Phase 3)
Instead of relying on the heavy, slow Linux TCP/IP networking stack (Netfilter, iptables, sockets), Nyx attaches an **eBPF (Extended Berkeley Packet Filter)** program directly to the hypervisor's network card via the Traffic Control (`tc`) ingress hook. It parses raw electrical network pulses, filters for specific TCP/HTTP traffic, measures the payload, and writes the packet data into a shared Ring-0 memory map for the Wasm engine to consume natively.

### 4. The Cage: LSM Zero-Trust Sandbox (Phase 4)
Because the Wasm VM operates in Ring-0, an exploit in the compiler itself could compromise the machine. Nyx deploys a secondary eBPF program attached to the kernel's **Linux Security Module (LSM)** hooks (e.g., `file_open`). This acts as an external failsafe cage. If the Wasm background thread attempts to open an unauthorized file or make a malicious network connection, the LSM eBPF hook intercepts the syscall in microseconds and hard-denies it with a `-EPERM` (Permission Denied) error.

---

## 📂 Repository Structure
```text
NYX/
├── wasm-engine/          # Heavily modified Wasm3 runtime (libc stripped, kmalloc wired)
├── nyx_init.c            # The primary Ring-0 Linux Kernel Module, kthread, and WALI bridge
├── nyx_net.bpf.c         # The eBPF network sensor and TCX packet parser
├── nyx_lsm.bpf.c         # The eBPF LSM Zero-Trust security cage
├── wali_test.wat         # Human-readable WebAssembly payload testing WALI
├── wasm_payload.h        # Auto-generated C-array of the compiled Wasm binary
├── Makefile              # Kernel module build instructions
└── README.md             # Project documentation
```

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
Compile the raw .wat (WebAssembly Text) file into a binary, and convert it to a C-header array so the kernel module can read it natively:
```bash
wat2wasm wali_test.wat -o wali_test.wasm
xxd -i wali_test.wasm > wasm_payload.h
```

### Step 2: Build and Inject the Ring-0 Engine
Compile the Linux Kernel Module and inject it into the live kernel. This will automatically spawn the `nyx_engine_thread` background heartbeat:
```bash
make clean
make
sudo insmod nyx_core.ko
```
Verify the engine ignited and the kthread is polling by checking the kernel logs:
```bash
sudo dmesg | tail -n 5
# Expected Output: 
# [NYX-CORE] Engine heartbeat started.
# [WALI-STDOUT] Wasm says: WALI live
```

### Step 3: Arm the eBPF Network Steerer
Compile the eBPF network sensor into restricted bytecode:
```bash
clang -g -O2 -target bpf -I/usr/include/x86_64-linux-gnu -c nyx_net.bpf.c -o nyx_net.bpf.o
```
Find your network interface name (`ip link`) and attach the eBPF program to the Traffic Control layer (replace `enp0s3` with your interface):
```bash
sudo tc qdisc add dev enp0s3 clsact
sudo tc filter add dev enp0s3 ingress bpf da obj nyx_net.bpf.o sec tc
```

### Step 4: Lock the LSM Security Cage
To compile the security cage, you must extract the `vmlinux.h` matrix from your live kernel:
```bash
sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
```
Compile the LSM hook and attach it to the kernel's security subsystem:
```bash
clang -g -O2 -target bpf -D__TARGET_ARCH_x86 -I. -c nyx_lsm.bpf.c -o nyx_lsm.bpf.o
sudo bpftool prog loadall nyx_lsm.bpf.o /sys/fs/bpf/nyx_lsm autoattach
```
Verify the cage is locked:
```bash
sudo bpftool prog list | grep lsm
# Expected: lsm  name nyx_restrict_files
```

### Step 5: Intercept the Matrix
Clear the kernel trace buffer, set up a real-time monitor, and fire an HTTP request.

**Terminal 1 (The Monitor):**
```bash
sudo sh -c 'echo > /sys/kernel/debug/tracing/trace'
sudo cat /sys/kernel/debug/tracing/trace_pipe | grep NYX-eBPF
```

**Terminal 2 (The Trigger):**
```bash
wget -qO- [http://example.com](http://example.com) > /dev/null
```

**Result in Terminal 1:**
```text
[NYX-eBPF] Captured HTTP Packet! Size: 60 bytes
[NYX-eBPF] Captured HTTP Packet! Size: 60 bytes
[NYX-eBPF] Captured HTTP Packet! Size: 891 bytes
```

### Step 6: Graceful Teardown
To safely kill the Ring-0 background thread, free the Wasm memory, and remove the module:
```bash
sudo rmmod nyx_core
```

## 🔮 Future Roadmap
* **XDP Hardware Drop:** Push the eBPF network steerer lower down the stack from Traffic Control (`tc`) into the eXpress Data Path (XDP) to process packets directly inside the network card driver.
* **Full WALI VFS Integration:** Expand the translation layer to intercept `fd_read` and `fd_write` to allow the sandboxed payload to interact securely with the host SSD via the kernel's Virtual File System.
* **Project Zyo Integration:** Tie the Unikernel kthread into a custom, AI-driven eBPF CPU scheduler to guarantee zero-jitter priority processing for incoming network payloads.

## 👤 Author
TheSNMC