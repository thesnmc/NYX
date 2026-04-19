#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kthread.h> // NEW: For background threads
#include <linux/delay.h>   // NEW: For sleeping

#include "wasm-engine/wasm3.h"
#include "wasm-engine/m3_env.h"
#include "wasm_payload.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nyx Architect");
MODULE_DESCRIPTION("Project Nyx: Threaded Ring-0 Wasm Unikernel");
MODULE_VERSION("1.0");

IM3Environment env = NULL;
IM3Runtime runtime = NULL;
IM3Module module = NULL;
IM3Function run_func = NULL;
static struct task_struct *nyx_thread; // Our engine's heartbeat

// WALI Translation Layer
m3ApiRawFunction(wali_print) {
    m3ApiGetArgMem(const char*, message); 
    printk(KERN_INFO "[WALI-STDOUT] Wasm says: %s\n", message);
    m3ApiSuccess();
}

// The continuous background polling loop
static int nyx_engine_loop(void *data) {
    printk(KERN_INFO "[NYX-CORE] Engine heartbeat started.\n");
    
    while (!kthread_should_stop()) {
        // In production, we'd check the eBPF map here.
        // If a packet is found, we execute the Wasm payload to process it.
        
        M3Result result = m3_CallV(run_func);
        if (result) printk(KERN_ERR "[NYX-FATAL] Wasm execution failed: %s\n", result);
        
        // Sleep for 5 seconds before checking the network again
        msleep(5000); 
    }
    
    printk(KERN_INFO "[NYX-CORE] Engine heartbeat stopped.\n");
    return 0;
}

static int __init nyx_init(void) {
    M3Result result = m3Err_none;
    printk(KERN_INFO "[NYX-CORE] Booting Unikernel...\n");

    env = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 8192, NULL);

    result = m3_ParseModule(env, &module, wali_test_wasm, wali_test_wasm_len);
    if (result) return -EINVAL;

    result = m3_LoadModule(runtime, module);
    if (result) return -EINVAL;

    m3_LinkRawFunction(module, "env", "print", "v(i)", &wali_print);
    m3_FindFunction(&run_func, runtime, "run");

    // Spawn the background thread!
    nyx_thread = kthread_run(nyx_engine_loop, NULL, "nyx_engine_thread");
    if (IS_ERR(nyx_thread)) {
        printk(KERN_ERR "[NYX-FATAL] Failed to spawn engine thread.\n");
        return PTR_ERR(nyx_thread);
    }

    return 0;
}

static void __exit nyx_exit(void) {
    // Stop the thread gracefully
    if (nyx_thread) kthread_stop(nyx_thread);
    if (runtime) m3_FreeRuntime(runtime);
    if (env) m3_FreeEnvironment(env);
    printk(KERN_INFO "[NYX-CORE] Unikernel offline.\n");
}

module_init(nyx_init);
module_exit(nyx_exit);