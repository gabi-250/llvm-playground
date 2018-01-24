.global restore_and_jmp

restore_and_jmp:
    mov    r,%rax
    mov    r+0x8,%rcx
    mov    r+0x10,%rdx
    mov    r+0x18,%rbx
    mov    r+0x30,%rsi
    mov    r+0x38,%rdi
    mov    r+0x40,%r8
    mov    r+0x48,%r9
    mov    r+0x50,%r10
    mov    r+0x58,%r11
    mov    r+0x60,%r12
    mov    r+0x68,%r13
    mov    r+0x70,%r14
    mov    r+0x78,%r15
    add    stack_size,%rsp
    pop    %rbp
    jmp    *addr