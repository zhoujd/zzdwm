/* Minimal x86-64 Linux assembly example for GNU as. */

    .text
    .globl _start

_start:
    // write(STDOUT_FILENO, message, message_len)
    mov $1, %rax
    mov $1, %rdi
    lea message(%rip), %rsi
    mov $message_len, %rdx
    syscall

    // _exit(0)
    mov $60, %rax
    xor %rdi, %rdi
    syscall

    .section .rodata
message:
    .ascii "Hello from Assembly!\n"
    .equ message_len, . - message
