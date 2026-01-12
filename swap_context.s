.type swap_context, @function
.global swap_context
swap_context:
  # Save the return address.
  movq (%rsp), %r8
  movq %r8, 8*0(%rdi)
  leaq 8(%rsp), %r8
  movq %r8, 8*1(%rdi)

  # Save preserved registers.
  movq %rbx, 8*2(%rdi)
  movq %rbp, 8*3(%rdi)
  movq %r15, 8*4(%rdi)
  movq %r14, 8*5(%rdi)
  movq %r13, 8*6(%rdi)
  movq %r12, 8*7(%rdi)

  # Should return to the address set with {get, swap}_context.
  movq 8*0(%rsi), %r8

  # Load new stack pointer.
  movq 8*1(%rsi), %rsp

  # Load preserved registers.
  movq 8*2(%rsi), %rbx
  movq 8*3(%rsi), %rbp
  movq 8*4(%rsi), %r15
  movq 8*5(%rsi), %r14
  movq 8*6(%rsi), %r13
  movq 8*7(%rsi), %r12

  # Push RIP to stack for RET.
  # pushq %r8

  # Return.
  xorl %eax, %eax
  # ret
  jmp *%r8

