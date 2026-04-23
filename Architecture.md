# 🏗️ Architecture & Design Document: Project NYX
**Version:** 1.0.0 | **Date:** 2026-04-23 | **Author:** TheSNMC

---

## 1. Executive Summary
This document outlines the architecture for Project Nyx, an experimental, bare-metal Composable Unikernel designed to completely bypass the traditional Ring-3 (user-space) to Ring-0 (kernel-space) privilege boundary. By embedding a WebAssembly (Wasm) virtual machine directly inside the Linux kernel, intercepting network traffic at the silicon level via eBPF XDP, and translating storage requests directly to the Virtual File System (VFS), Nyx executes stateless microservices at native hardware velocities without a single user-space context switch.

## 2. Architectural Drivers
**What forces shaped this architecture?**

* **Primary Goals:** Absolute maximum execution speed, zero context-switching latency, and the ability to run untrusted, modular microservices natively in Ring-0.
* **Technical Constraints:** Must operate entirely within the Linux Kernel space without access to the standard C library (`libc`). The core engine must adhere to strict GPLv2 licensing to access `EXPORT_SYMBOL_GPL` kernel APIs.
* **Non-Functional Requirements (NFRs):** * **Security:** Must guarantee system stability despite running untrusted code in the kernel. Achieved via WebAssembly's mathematical memory sandboxing and an external eBPF Linux Security Module (LSM) failsafe cage.
  * **Performance:** Must intercept and parse network packets before the host OS allocates memory structures (bypassing `sk_buff` generation).
  * **Data Sovereignty:** Payloads must remain entirely stateless and mathematically isolated from host system memory unless explicitly bridged.

## 3. System Architecture (The 10,000-Foot View)
Project Nyx is divided into four highly specialized, independent pillars operating entirely in Ring-0:

* **The Engine Layer (Wasm Core):** A heavily modified `wasm3` runtime compiled as a standalone Linux Kernel Module (`nyx_core.ko`). It replaces standard `malloc` with kernel-safe `kmalloc` and runs continuously on a background kthread heartbeat.
* **The Translator Layer (WALI):** The WebAssembly Linux Interface. A custom C-bridge that intercepts WebAssembly System Interface (WASI) calls (like `fd_read` and `fd_write`) and maps them directly to the kernel's Virtual File System (`filp_open`, `kernel_read`), granting raw SSD access without syscalls.
* **The Steerer Layer (XDP Network Drop):** An eBPF program injected directly into the Network Interface Card (NIC) driver via the eXpress Data Path (XDP). It filters raw electrical pulses for target HTTP traffic and writes packet metadata into a shared Ring-0 memory map.
* **The Cage Layer (LSM Zero-Trust):** A secondary eBPF program attached to the kernel's security subsystem (LSM). It dynamically restricts the Wasm module's execution paths, intercepting unauthorized `file_open` or socket requests in microseconds and issuing hard `-EPERM` denials.

## 4. Design Decisions & Trade-Offs (The "Why")

* **Decision 1: Running Untrusted Code in Ring-0 via WebAssembly**
  * **Rationale:** Traditional Docker containers suffer from massive latency due to constant `copy_from_user` boundary crossing. WebAssembly acts as a virtual Instruction Set Architecture (ISA) with strict, mathematically proven memory safety boundaries, allowing native-speed execution without traditional hardware rings.
  * **Trade-off:** A single bug in the Wasm runtime's C-wrapper will result in an unrecoverable Kernel Panic, taking down the entire host machine.

* **Decision 2: eBPF XDP over Traffic Control (TC)**
  * **Rationale:** Pushing the network sensor down to the XDP layer processes packets on the physical silicon buffer, bypassing the heavy Linux TCP/IP stack entirely.
  * **Trade-off:** We lose access to the kernel's `sk_buff` convenience structure, forcing us to manually calculate packet sizes and offsets using raw memory pointers (`data_end - data`), which drastically increases the complexity of the C code.

* **Decision 3: Split Licensing (GPLv2 Engine / Proprietary Payloads)**
  * **Rationale:** The Linux kernel violently rejects proprietary code attempting to access deep internal functions like VFS and eBPF maps. The core `nyx_core` module must be GPLv2.
  * **Trade-off:** We open-source the hypervisor/engine, but because the Wasm payloads run in an isolated VM and only interact with our WALI bridge, enterprise users can compile and run 100% closed-source, proprietary microservices inside the Unikernel.

## 5. Data Flow & Lifecycle

* **Ingestion (The Silicon Drop):** Raw network electrical pulses hit the NIC. The `nyx_net.bpf.c` XDP program intercepts them, identifies HTTP traffic, calculates the size, and writes the metric into the `nyx_packet_map` shared Ring-0 memory.
* **Processing (The Heartbeat):** The `nyx_init.c` background kthread detects the new packet in the shared map. It triggers the compiled `.wasm` payload, injecting the data into the mathematically isolated Wasm memory array.
* **Execution/Output (The VFS Breach):** The Wasm payload processes the data. If storage is required, it calls the internal `write_file` WALI function. WALI bypasses the syscall table and writes the bytes physically to the host SSD via `kernel_write`.

## 6. Security & Privacy Threat Model

* **Ring-0 Escape Risk:** The primary threat is a malicious payload breaking out of the Wasm memory boundaries.
  * **Mitigation 1 (Mathematical):** The Wasm ISA inherently prevents out-of-bounds memory access.
  * **Mitigation 2 (The Cage):** If a compiler exploit occurs, the `nyx_lsm.bpf.c` security cage intercepts the resulting rogue kernel syscalls at the LSM layer and hard-kills the process.
* **Data in Transit:** System operates natively on raw hardware buffers; payload data is completely air-gapped from user-space memory scraping tools.

## 7. Future Architecture Roadmap

* **Symmetric Multiprocessing (SMP) Scaling:** Refactoring the kthread engine loop to dynamically map to the host's CPU core count, spawning dedicated, isolated Wasm environments for massive parallel network processing.
* **Quantum RNG Memory Injection:** Expanding the WALI translation layer to interface directly with hardware entropy pools (QRNG), allowing the Ring-0 payloads to execute native post-quantum cryptographic hashing.
* **Project Zyo Integration:** Bridging the Nyx engine into an AI-driven eBPF CPU scheduler to guarantee absolute, zero-jitter priority processing for Wasm network payloads over standard host OS processes.
