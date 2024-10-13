	.global _start
	.global _exit

	.section .text.start
_start:
	mov	#__etext, r4
	mov	#__data, r5
	jmp	2f
1:	mov	@r4+, @r5
	incd	r5
2:	cmp	#__edata, r5
	jne	1b

__do_clear_bss:
	mov	#__ebss, sp
	jmp	2f
1:	push	#0
2:	cmp	#__bss, sp
	jne	1b

__setup_stack:
	mov	#__stack, sp
	call	#main

_exit:
3:      mov	#0, &0xfffe
	dint
	nop
	bis	#0x10, sr
	jmp	3b
