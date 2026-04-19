(module
  ;; Import the WALI print function from the host (our kernel)
  (import "env" "print" (func $print (param i32)))
  
  ;; Allocate 1 page of sandboxed memory
  (memory 1)
  
  ;; Store our string in the Wasm memory at offset 0
  (data (i32.const 0) "WALI live\00")
  
  ;; The main function we will execute
  (func (export "run")
    i32.const 0  ;; Push memory offset 0 onto the stack
    call $print  ;; Trigger the system call intercept!
  )
)