# Target module name
obj-m += nyx_core.o

# Combine our init file and all wasm engine files into one super-module
nyx_core-y := nyx_init.o \
    wasm-engine/m3_bind.o \
    wasm-engine/m3_code.o \
    wasm-engine/m3_compile.o \
    wasm-engine/m3_core.o \
    wasm-engine/m3_env.o \
    wasm-engine/m3_exec.o \
    wasm-engine/m3_function.o \
    wasm-engine/m3_info.o \
    wasm-engine/m3_module.o \
    wasm-engine/m3_parse.o

# Inject our compatibility header into every file automatically
ccflags-y := -I$(PWD)/wasm-engine -include $(PWD)/nyx_wasm_compat.h -Dd_m3HasFloat=0 -Wno-declaration-after-statement

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean