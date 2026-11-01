.text
.globl _start
_start:
#ifdef __i386__
    movl $1, %eax
    xorl %ebx, %ebx
    int $0x80
#else
    movl $60, %eax
    xorl %edi, %edi
    syscall
#endif
    ud2
.section .note.GNU-stack,"",@progbits
