# 🌌 Project NYX
> A unified Ring-0 WebAssembly composable Unikernel that processes network payloads and disk I/O at native hardware speeds without a single user-space context switch.

---

## 📖 Overview
Traditionally, operating systems isolate untrusted user applications in "user space" (Ring-3) and keep highly privileged core operations in "kernel space" (Ring-0). Whenever a microservice or container needs network or disk access, it triggers a system call, forcing the CPU to context-switch between these rings. This switching, along with copying data back and forth via functions like `copy_from_user`, introduces massive latency bottlenecks in modern cloud infrastructure.

Project Nyx is an experimental, bare-metal systems architecture that completely shatters this boundary. By embedding a WebAssembly (Wasm) virtual machine directly inside the Linux kernel as a standalone module, Nyx allows untrusted user payloads to execute in Ring-0 at native hardware speeds. Because WebAssembly acts as a virtual Instruction Set Architecture (ISA) protected by strict mathematical memory boundaries, it does not need to rely on traditional hardware-level privilege rings to ensure safety.

Coupled with eBPF network interception and Linux Security Module (LSM) cages, Nyx represents the absolute bleeding edge of stateless, extreme-performance cloud infrastructure—bypassing traditional containerization entirely.

**The Core Mandate:** Total bypass of the Ring-3 user-space stack to achieve maximum hardware velocity, utilizing strict mathematical memory sandboxing, silicon-level packet drops, and zero-trust kernel execution.

## ✨ Key Features
* **In-Kernel WebAssembly Engine:** A custom-ported `wasm3` runtime stripped of user-space dependencies, wired directly to kernel memory allocators (`kmalloc`), and running autonomously on a continuous kthread heartbeat.
* **WALI VFS Storage Integration:** The WebAssembly Linux Interface (WALI) intercepts sandboxed `fd_read` and `fd_write` calls and maps them seamlessly to the kernel's Virtual File System (`filp_open`, `kernel_read`), granting raw SSD access without system calls.
* **eBPF XDP Silicon Drop:** Network traffic is intercepted at the absolute lowest level—the eXpress Data Path (XDP). Raw HTTP packets are parsed directly off the Network Interface Card (NIC) driver's electrical buffer before the Linux kernel even allocates memory for them.
* **LSM Zero-Trust Security Cage:** A secondary eBPF program attaches to the kernel's Linux Security Module (LSM) hooks. It acts as an external failsafe, instantly hard-denying unauthorized file or network actions with a `-EPERM` error if the Wasm payload attempts to breach its parameters.

## 🛠️ Tech Stack
* **Language:** C (Kernel Module), WebAssembly (WAT/WASM), eBPF
* **Framework:** Custom `wasm3` Ring-0 runtime, WALI (WebAssembly Linux Interface)
* **Environment:** Linux Kernel (6.x+), Clang / LLVM, Bare-metal or isolated VM
* **Key Libraries/APIs:** `linux/bpf.h`, `linux/fs.h`, `vmlinux.h` (dynamically extracted BTF), eBPF XDP & TC.

## ⚙️ Architecture & Data Flow
Project Nyx fundamentally reimagines the data lifecycle by keeping every operation within Ring-0 memory limits.

* **Input:** The `nyx_net.bpf.c` XDP program parses raw packets directly from the NIC hardware buffer. If it detects a target HTTP payload, it calculates the raw size and writes it into a shared eBPF Ring-0 memory map.
* **Processing:** The `nyx_init.c` background kthread polls the shared memory map. Upon detecting a packet, it executes the WebAssembly payload (`wasm_payload.h`) natively in the kernel. The Wasm payload computes the necessary logic in complete mathematical isolation.
* **Output:** If the payload requires storage, it calls the WALI bridge, bypassing standard syscalls to write the computed data directly to the host SSD via the internal Virtual File System (VFS).

## 🔒 Privacy & Data Sovereignty
* **Data Collection:** Absolute zero. This is a bare-metal kernel architecture designed for sovereign, stateless data processing. Data is processed in volatile kernel memory and immediately discarded or routed.
* **Permissions Required:** Root (`sudo`) is strictly required to load the `.ko` kernel module, attach the XDP hooks to the network interface, and load the eBPF LSM objects.
* **Cloud Connectivity:** Completely agnostic. Nyx operates independently of any external orchestration layers, cloud providers, or foreign infrastructure grids.

## 🚀 Getting Started

### ⚠️ CRITICAL WARNING
This project involves loading unsigned, highly privileged C code directly into Ring-0. A single memory violation can result in an immediate, unrecoverable Kernel Panic. **Do not run this on a host machine, daily driver, or production server.** Use an isolated Virtual Machine.

### Prerequisites
* **Minimum OS:** Linux Kernel 6.x+ with BTF (BPF Type Format) enabled.
* **Required development environment:**
  ```bash
  sudo apt update
  sudo apt install build-essential linux-headers-$(uname -r)
  sudo apt install clang llvm libbpf-dev linux-tools-common linux-tools-generic linux-tools-$(uname -r)
  sudo apt install wabt
  ```

### Installation

1. **Clone the repository:**
   ```bash
   git clone [https://github.com/thesnmc/NYX.git](https://github.com/thesnmc/NYX.git)
   cd NYX
   ```

2. **Forge the WebAssembly Payload:**
   Compile the raw WebAssembly Text into a binary and convert it to a C-header array:
   ```bash
   wat2wasm wali_test.wat -o wali_test.wasm
   xxd -i wali_test.wasm > wasm_payload.h
   ```

3. **Build and Inject the Ring-0 Engine:**
   ```bash
   make clean && make
   sudo insmod nyx_core.ko
   # Verify the heartbeat:
   sudo dmesg | tail -n 5
   ```

4. **Arm the XDP Network Steerer:**
   Compile the silicon-level sensor and attach it to your network interface (replace `enp0s3` with your interface name):
   ```bash
   clang -g -O2 -target bpf -I/usr/include/x86_64-linux-gnu -c nyx_net.bpf.c -o nyx_net.bpf.o
   sudo ip link set dev enp0s3 xdp obj nyx_net.bpf.o sec xdp
   ```

5. **Lock the LSM Security Cage:**
   Extract the kernel's BTF matrix and attach the zero-trust cage:
   ```bash
   sudo bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
   clang -g -O2 -target bpf -D__TARGET_ARCH_x86 -I. -c nyx_lsm.bpf.c -o nyx_lsm.bpf.o
   sudo bpftool prog loadall nyx_lsm.bpf.o /sys/fs/bpf/nyx_lsm autoattach
   ```

6. **Graceful Teardown:**
   ```bash
   sudo ip link set dev enp0s3 xdp off
   sudo rmmod nyx_core
   ```

## 🤝 Contributing
Contributions, issues, and feature requests are welcome. Feel free to check the issues page if you want to contribute. Future roadmaps include Symmetric Multiprocessing (SMP) scaling across CPU cores and WALI integration for Quantum RNG memory injection.

## 📄 License
This project's core Ring-0 kernel module (`nyx_core`) is licensed under GPL v2 to ensure strict compatibility with the Linux Kernel's `EXPORT_SYMBOL_GPL` APIs. The WebAssembly payloads and user-space components are dual-licensed or open to proprietary modifications. See the LICENSE file for details.  
Built by an independent developer in Chennai, India.
