#ifndef NYX_WASM_COMPAT_H
#define NYX_WASM_COMPAT_H

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/bitops.h> // NEW: Kernel Bitwise Math

// 1. Map standard C types to Linux Kernel types
typedef u8 uint8_t;
typedef u16 uint16_t;
typedef u32 uint32_t;
typedef u64 uint64_t;
typedef s8 int8_t;
typedef s16 int16_t;
typedef s32 int32_t;
typedef s64 int64_t;
typedef size_t uintptr_t;

// 2. Inject Missing Mathematical Boundaries & Booleans
#define CHAR_BIT 8

#undef INT_MAX
#undef INT_MIN
#define INT_MAX 2147483647
#define INT_MIN (-INT_MAX - 1)

#define INT32_MAX 2147483647
#define INT32_MIN (-INT32_MAX - 1)
#define INT64_MAX 9223372036854775807LL
#define INT64_MIN (-INT64_MAX - 1LL)

#undef true
#undef false
#define true 1
#define false 0

// 3. Hot-wire memory allocation to the kernel heap
#define malloc(size) kmalloc(size, GFP_KERNEL)
#define calloc(count, size) kcalloc(count, size, GFP_KERNEL)
#define free(ptr) kfree(ptr)
#define realloc(ptr, size) krealloc(ptr, size, GFP_KERNEL)

// 4. Map printing to the kernel ring buffer
#define printf(...) printk(KERN_INFO "[WASM3] " __VA_ARGS__)

// 5. Disable Floating Point Math (FPU context switching in Ring-0 is highly dangerous)
#define d_m3HasFloat 0

// 6. Provide a kernel-safe assert
#define d_m3Assert(x) if(!(x)) printk(KERN_ERR "[NYX-PANIC] WASM ASSERT FAILED: " #x "\n")

// 7. Map String Parsers
#define strtoul simple_strtoul
#define strtoull simple_strtoull

// 8. Map Compiler Math Built-ins to Kernel Equivalents (The Linker Fix)
#define __builtin_popcount hweight32
#define __builtin_popcountll hweight64

#endif