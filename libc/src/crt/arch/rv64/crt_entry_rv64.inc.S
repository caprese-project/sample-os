.extern __global_pointer$
.type __global_pointer$, @object
.extern __crt_startup
.type __crt_startup, @function
.extern __init_context
.type __init_context, @object
.extern main
.type main, @function
.extern exit
.type exit, @function

.section .text.entry
.global _start
.type _start, @function
_start:
  # Without norelax option, "auipc gp, %pcrel_hi(__global_pointer$)" is optimized to "auipc gp, gp".
  .option push
  .option norelax
    1:
      auipc gp, %pcrel_hi(__global_pointer$)
      addi  gp, gp, %pcrel_lo(1b)
  .option pop

  la t0, __init_context
  sd a0, 0(t0)  # argc
  sd a1, 8(t0)  # argv
  sd a2, 16(t0) # this-task cap
  sd a3, 24(t0) # apm-task cap
  sd a4, 32(t0)
  sd a5, 40(t0)
  sd a6, 48(t0)
  sd a7, 56(t0)

  mv s0, a0
  mv s1, a1
  call __crt_startup
  bnez a0, exit_phase
  mv a0, s0
  mv a1, s1
  call main

exit_phase:
  j exit
