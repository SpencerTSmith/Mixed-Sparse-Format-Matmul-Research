global read32_asm
global read64_asm
global read128_asm
global read256_asm
global read512_asm
global fmadd_asm

section .text

read256_asm:
  align 64
.loop:
  vmovdqu ymm0, [rsi]
  vmovdqu ymm1, [rsi + 32]
  add rsi, 64
  sub rdi, 64
  jnle .loop
  ret

; Try to get full saturation
fmadd_asm:
  vbroadcastsd ymm0, [rel one]
  vbroadcastsd ymm1, [rel one]

  align 64
.loop:
  vfmadd231pd ymm2, ymm0, ymm0
  vfmadd231pd ymm3, ymm1, ymm1

  ; 2 fmadds (2 flops), 4 wide each, so 2 * 4 * 2 = 16
  sub rdi, 16
  jnz .loop

  ret

section .data
one: dq 1.0
