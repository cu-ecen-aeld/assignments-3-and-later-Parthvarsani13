# Kernel Oops Analysis - Assignment 7

## **0. Run the command**
- **echo “hello_world” > /dev/faulty**

## **1. Kernel Oops Details**
- **Error Type:** NULL Pointer Dereference
- **Faulting Address:** `0x0000000000000000` (NULL)
- **Process Affected:** `sh` (Shell)
- **Error Code (ESR):** `0x96000045`
- **Function Causing the Fault:** `faulty_write+0x10/0x20 [faulty]`
- **Call Trace:**
  ```
  faulty_write+0x10/0x20 [faulty]
  ksys_write+0x74/0x110
  __arm64_sys_write+0x1c/0x30
  invoke_syscall+0x54/0x130
  el0_svc_common.constprop.0+0x44/0xf0
  do_el0_svc+0x2c/0xc0
  el0_svc+0x2c/0x90
  el0t_64_sync_handler+0xf4/0x120
  el0t_64_sync+0x18c/0x190
  ```

## **2. Cause of the Oops**
The kernel oops was triggered by a **NULL pointer dereference** in the `faulty_write` function of the **faulty** module.

- The crash happened because `faulty_write` attempted to write to memory at address `0x0000000000000000`, which is an **invalid memory access**.
- The function did not check whether a pointer was `NULL` before dereferencing it.

### **Memory Abort Analysis**
- **Error Code (EC):** `0x25: DABT (current EL), IL = 32 bits`
- **FSC:** `0x05: level 1 translation fault` → This means the memory access was invalid at Level 1 of the page table hierarchy.

## **3. How to Prevent This Issue**
To prevent a NULL pointer dereference:
1. **Check if the pointer is valid before accessing it.**
   ```c
   if (!ptr) {
       printk(KERN_ERR "Error: NULL pointer dereference in faulty_write\n");
       return -EFAULT;
   }
   ```
2. **Use Kernel Address Sanitizer (KASAN) to detect invalid accesses.**
   - Enable KASAN in the kernel config (`CONFIG_KASAN=y`).
3. **Use Debugging Techniques:**
   - Check dmesg logs (`dmesg | tail -n 50`).
   - Use `gdb` with `objdump -D` to analyze function offsets.
   - Run the kernel with `kgdb` or `ftrace` for deeper analysis.

## **4. References**
- [Kernel Oops Debugging Guide](https://www.kernel.org/doc/html/latest/admin-guide/bug-hunting.html)
- Weekly lecture discussion on **kernel memory faults**
- [Mastering Markdown](https://guides.github.com/features/mastering-markdown/)
