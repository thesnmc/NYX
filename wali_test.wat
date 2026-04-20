(module
  ;; 1. Import our custom WALI VFS and Print functions from the Ring-0 host
  (import "env" "print" (func $print (param i32)))
  (import "env" "read_file" (func $read_file (param i32 i32 i32) (result i32)))
  (import "env" "write_file" (func $write_file (param i32 i32 i32) (result i32)))

  ;; 2. Allocate 1 page (64KB) of strict sandboxed memory
  (memory 1)

  ;; 3. THE MEMORY MAP
  ;; Offset 0: The target filepath on the host Linux machine
  (data (i32.const 0) "/tmp/nyx_proof.txt\00")
  
  ;; Offset 50: The payload we want to write to the SSD
  (data (i32.const 50) "VFS successfully breached from Ring-0 Wasm Sandbox!\00")
  
  ;; Offset 150: An empty buffer reserved for reading the file back
  ;; Offset 250: A success notification message
  (data (i32.const 250) "WALI VFS Test Complete\00")

  ;; 4. THE EXECUTION THREAD
  (func (export "run")
    
    ;; STEP A: WRITE TO SSD
    ;; Call: write_file(filepath_ptr=0, input_ptr=50, write_bytes=52)
    i32.const 0
    i32.const 50
    i32.const 52
    call $write_file
    drop ;; Drop the returned bytes_written count from the stack

    ;; STEP B: READ FROM SSD
    ;; Call: read_file(filepath_ptr=0, output_ptr=150, max_bytes=60)
    i32.const 0
    i32.const 150
    i32.const 60
    call $read_file
    drop ;; Drop the returned bytes_read count

    ;; STEP C: PRINT THE READ BUFFER TO KERNEL LOG
    ;; This proves we actually retrieved the data from the SSD!
    i32.const 150
    call $print

    ;; STEP D: PRINT SUCCESS
    i32.const 250
    call $print
  )
)