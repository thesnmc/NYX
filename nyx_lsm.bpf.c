#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

// Attach to the kernel's internal "file_open" security hook
SEC("lsm/file_open")
int BPF_PROG(nyx_restrict_files, struct file *file) {
    // Get the name of the program trying to open a file
    char task_comm[16];
    bpf_get_current_comm(&task_comm, sizeof(task_comm));

    // If our background Wasm thread ("nyx_engine_thre") tries to open a file...
    if (task_comm[0] == 'n' && task_comm[1] == 'y' && task_comm[2] == 'x') {
        
        bpf_printk("[NYX-LSM] SEC_VIOLATION: Wasm engine attempted to access the filesystem!\n");
        
        // Instantly deny the operation with a "Permission Denied" (-EPERM) error
        return -1; // -EPERM
    }

    // Allow all other normal Linux processes to open files
    return 0; 
}

char _license[] SEC("license") = "GPL";