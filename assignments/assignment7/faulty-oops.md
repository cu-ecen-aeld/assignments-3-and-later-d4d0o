Entering the command:

```
echo “hello_world” > /dev/faulty
```

result in a kernel oops (see bellow the oops details). 

Indication of an access of memory 0, NULL pointer is indicated by

> NULL pointer dereference at virtual address 0000000000000000

The module faulty and the function faulty_write are given by

> pc : faulty_write+0x10/0x20 \[faulty\]

::: info
*16 (0x10) bytes into the function faulty_write, which is 32 (0x20) bytes long*

:::

Which help to point in **ssize_t faulty_write (...** the following line:

```
(int *)0 = 0;
```

##### Kernel oops details:

::: warn
Unable to handle kernel NULL pointer dereference at virtual address 0000000000000000
Mem abort info:
ESR = 0x0000000096000045
EC = 0x25: DABT (current EL), IL = 32 bits
SET = 0, FnV = 0
EA = 0, S1PTW = 0
FSC = 0x05: level 1 translation fault
Data abort info:
ISV = 0, ISS = 0x00000045
CM = 0, WnR = 1
user pgtable: 4k pages, 39-bit VAs, pgdp=000000004128f000
\[0000000000000000\] pgd=0000000000000000, p4d=0000000000000000, pud=0000000000000000
Internal error: Oops: 0000000096000045 \[#1\] SMP
Modules linked in: scull(O) faulty(O) hello(O)
CPU: 0 PID: 107 Comm: sh Tainted: G           O       6.1.26 #1
Hardware name: linux,dummy-virt (DT)
pstate: 80000005 (Nzcv daif -PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : faulty_write+0x10/0x20 \[faulty\]
lr : vfs_write+0xc4/0x350
sp : ffffffc008cfbd20
x29: ffffffc008cfbd20 x28: ffffff8001b8ce00 x27: 0000000000000000
x26: 0000000000000000 x25: 0000000000000000 x24: 0000000000000000
x23: 0000000000000012 x22: 0000000000000012 x21: ffffffc008cfbdf0
x20: 0000005582a04e30 x19: ffffff8001ae6800 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000000000000000
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000000 x10: 0000000000000000 x9 : 0000000000000000
x8 : 0000000000000000 x7 : 0000000000000000 x6 : 0000000000000000
x5 : 0000000000000001 x4 : ffffffc000705000 x3 : ffffffc008cfbdf0
x2 : 0000000000000012 x1 : 0000000000000000 x0 : 0000000000000000
Call trace:
faulty_write+0x10/0x20 \[faulty\]
ksys_write+0x68/0x100
\__arm64_sys_write+0x1c/0x30
invoke_syscall+0x54/0x130
el0_svc_common.constprop.0+0x44/0x100
do_el0_svc+0x30/0xc0
el0_svc+0x2c/0x90
el0t_64_sync_handler+0xbc/0x140
el0t_64_sync+0x18c/0x190
Code: d2800001 d2800000 d503233f d50323bf (b900003f)
\---\[ end trace 0000000000000000 \]---

:::