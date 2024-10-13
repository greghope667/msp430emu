	.text
	.global	main

.equ	UART,	0xffa2

main:
	mov	#str, r4
1:	mov.b	@r4+, r5
	mov	r5, &UART
	cmp.b	#0, r5
	jne	1b
	ret

	.section .data
str:
	.asciz	"hello\n"
