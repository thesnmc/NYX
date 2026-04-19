#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

// Bring in our Wasm Engine
#include "wasm-engine/wasm3.h"
#include "wasm-engine/m3_env.h"

// Bring in our auto-compiled Wasm binary!
#include "wasm_payload.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nyx Architect");
MODULE_DESCRIPTION("Project Nyx: WALI Translation Layer");
MODULE_VERSION("0.4");

// Pointers to keep the engine alive during module lifespan
IM3Environment env = NULL;
IM3Runtime runtime = NULL;
IM3Module module = NULL;
IM3Function run_func = NULL;

// ============================================================================
// WALI: WebAssembly Linux Interface
// Intercepts Wasm calls and translates them to Kernel operations
// ============================================================================
m3ApiRawFunction(wali_print)
{
    m3ApiGetArgMem(const char*, message); 
    
    // Wasm just passed us '0' (the memory offset).
    // The engine's macro automatically translates that '0' into the actual 
    // physical memory address where the Wasm sandbox lives in the kernel.
    
    printk(KERN_INFO "[WALI-STDOUT] %s\n", message);
    
    m3ApiSuccess();
}
// ============================================================================

static int __init nyx_init(void) {
    M3Result result = m3Err_none;

    printk(KERN_INFO "[NYX-CORE] Entering Ring-0...\n");

    // 1. Initialize the Environment & Runtime
    env = m3_NewEnvironment();
    runtime = m3_NewRuntime(env, 8192, NULL);

    // 2. Parse and Load the Module using the auto-generated array
    result = m3_ParseModule(env, &module, wali_test_wasm, wali_test_wasm_len);
    if (result) goto fatal_error;

    result = m3_LoadModule(runtime, module);
    if (result) goto fatal_error;

    // 3. THE WALI BINDING: Map the Wasm import to our C function
    // "v(i)" tells the engine the function returns void (v) and takes one integer (i)
    result = m3_LinkRawFunction(module, "env", "print", "v(i)", &wali_print);
    if (result) {
        printk(KERN_ERR "[NYX-FATAL] Failed to bind WALI interface: %s\n", result);
        return -EINVAL;
    }

    // 4. Find the "run" function
    result = m3_FindFunction(&run_func, runtime, "run");
    if (result) goto fatal_error;

    printk(KERN_INFO "[NYX-CORE] Triggering WALI execution...\n");

    // 5. Execute!
    result = m3_CallV(run_func);
    if (result) goto fatal_error;

    return 0;

fatal_error:
    printk(KERN_ERR "[NYX-FATAL] Execution failed: %s\n", result);
    return -EFAULT;
}

static void __exit nyx_exit(void) {
    if (runtime) m3_FreeRuntime(runtime);
    if (env) m3_FreeEnvironment(env);
    printk(KERN_INFO "[NYX-CORE] Exiting Ring-0.\n");
}

module_init(nyx_init);
module_exit(nyx_exit);