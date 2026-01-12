.type setup_context, @function
.global setup_context
setup_context:
    # First (%rdi) argument = rsp (stack base pointer)
    # Second (%rsi) argument = rip (trampoline function pointer)
    # Third (%rdx) argument = trampoline function context argument
    movq %rsp, %r8 # save current rsp
    movq %rdi, %rsp

    andq $-16, %rsp

    subq $8, %rsp

    pushq %rdx
    
    pushq $1

    movq %rsp, %rax

    movq %r8, %rsp

    ret



