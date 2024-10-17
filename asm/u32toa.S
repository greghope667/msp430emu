	.global	main
	.text

#define	UART	0xffa2

main:
	call	#to_base_10
	call	#print
	ret
	
to_base_10:
	mov	number, r15		; Low word
	mov	number+2, r14		; High word
	mov	#32, r8 		; Iteration counter
.insert_bit:

	; Fixups
	mov	#5, r12			; Iterate through scratch buffer
.fixup_add3:
	mov.b	scratch-1(r12), r11	; Get BCD pair from buffer
	cmp	#80, r11		; If digit >= 5
	jlo	1f
	add	#48, r11		;     digit += 3
1:	mov	#15, r10
	and	r11, r10
	cmp	#5, r10			; If digit >= 5
	jlo	2f
	add	#3, r11			;     digit += 3
2:	mov.b	r11, scratch-1(r12)	; Store fixed up pair
	dec	r12
	jnz	.fixup_add3

	; Shift all bits
	rla	r15
	rlc	r14
	mov	#scratch, r12
	addc	@r12+, -2(r12)
	addc	@r12+, -2(r12)
	addc.b	@r12+, -1(r12)

	dec	r8
	jnz	.insert_bit
	
	ret

print:
	mov	#5, r12			; Iterate through scratch buffer
.loop:
	mov.b	scratch-1(r12), r11	; Get BCD pair from buffer
	mov	r11, r10

	rra	r11
	rra	r11
	rra	r11
	rra	r11			; High BCD digit
	bis	#0x30, r11		; To ASCII
	mov	r11, &UART

	and	#0xf, r10		; Low BCD digit
	bis	#0x30, r10		; To ASCII
	mov	r10, &UART

	dec	r12
	jnz	.loop

	ret
	
	.data
number:
	.long	123456789

	.bss
scratch:
	.zero	5
