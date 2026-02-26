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
  vbroadcastsd ymm2, [rel one]
  vbroadcastsd ymm3, [rel one]
  vbroadcastsd ymm4, [rel one]
  vbroadcastsd ymm5, [rel one]
  vbroadcastsd ymm6, [rel one]
  vbroadcastsd ymm7, [rel one]

  align 64
.loop:
  vfmadd231pd ymm0, ymm0, ymm0
  vfmadd231pd ymm1, ymm1, ymm1
  vfmadd231pd ymm2, ymm2, ymm2
  vfmadd231pd ymm3, ymm3, ymm3
  vfmadd231pd ymm4, ymm4, ymm4
  vfmadd231pd ymm5, ymm5, ymm5
  vfmadd231pd ymm6, ymm6, ymm6
  vfmadd231pd ymm7, ymm7, ymm7

  ; 8 fmadds (2 flops), 4 wide each, so 8 * 4 * 2 = 64
  sub rdi, 64
  jnz .loop

  ret

section .data
one: dq 1.0
