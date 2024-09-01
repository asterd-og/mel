[default rel]
[section .text]
[global print]
[global printInt]
[global printChar]
[global allocMem]
[global freeMem]
[extern printf]
[extern malloc]
[extern free]

print:
  push rbp
  mov rbp, rsp
  call printf wrt ..plt
  mov rsp, rbp
  pop rbp
  ret

printInt:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov qword [rbp-8], rdi ; Number
  lea rdi, [LC0]
  mov rsi, qword [rbp-8]
  call printf wrt ..plt
  mov rsp, rbp
  pop rbp
  ret

printChar:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov qword [rbp-8], rdi ; Number
  lea rdi, [LC1]
  mov rsi, qword [rbp-8]
  call printf wrt ..plt
  mov rsp, rbp
  pop rbp
  ret

allocMem:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov qword [rbp-8], rdi
  mov rdi, qword [rbp-8]
  call malloc wrt ..plt
  mov rsp, rbp
  pop rbp
  ret

freeMem:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov qword [rbp-8], rdi
  mov rdi, qword [rbp-8]
  call free wrt ..plt
  mov rsp, rbp
  pop rbp
  ret

[section .data]
LC0: db `%ld`,0
LC1: db `%c`,0

