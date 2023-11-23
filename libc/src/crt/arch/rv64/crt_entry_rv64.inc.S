.extern __global_pointer$
.type __global_pointer$, @object
.extern __crt_startup
.type __crt_startup, @function
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

  # a0: argc
  # a1: argv
  # s0: this task cap
  # s1: apm ep cap
  # s2: heap start

  mv s3, a0
  mv s4, a1
  mv a0, s0
  mv a1, s1
  mv a2, s2
  call __crt_startup
  bnez a0, exit_phase
  mv a0, s3
  mv a1, s4
  call main

exit_phase:
  j exit
