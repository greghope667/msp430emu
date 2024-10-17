	.global	main
	.text

#define	UART	0xffa2

main:
	mov	&number, r12
	mov	&number+2, r13

.L:
	call	#divmod10
	bis	#0x30, r14
	mov	r14, &UART
	cmp	#0, r12
	jnz	.L
	cmp	#0, r13
	jnz	.L

	ret
	
divmod10:
	; Divide/mod 32 bit number by 10
	; Input in r12:r13
	; Output divide r12:r13
	; Output mod r14

	push	r9
	push	r8

	; Local q = r12:r13
	; Local x = r8:r9
	; Local in = r14

	; Save in
	mov	r12, r14

	; x = in|1
	mov	#1, r8
	bis	r12, r8
	mov	r13, r9

	; x = x - (in>>2)
	clrc
	rrc	r13
	rrc	r12
	rra	r13
	rrc	r12
	sub	r12, r8
	subc	r13, r9

	; q = (x>>4) + x
	mov	r8, r12
	mov	r9, r13
	clrc
	rrc	r13
	rrc	r12
	rra	r13
	rrc	r12
	rra	r13
	rrc	r12
	rra	r13
	rrc	r12
	add	r8, r12
	addc	r9, r13

	; x = q
	mov	r12, r8
	mov	r13, r9

	; Loop 4, counter = r11
	mov	#4, r11
.shr8:
	; q = (q>>8)
	swpb	r13
	swpb	r12
	mov.b	r13, r15
	and	#0xff00, r13
	and	#0x00ff, r12
	bis	r13, r12
	mov	r15, r13

	; q = q + x
	add	r8, r12
	addc	r9, r13

	dec	r11
	jnz	.shr8

	; div = q >> 3
	clrc
	rrc	r13
	rrc	r12
	rra	r13
	rrc	r12
	rra	r13
	rrc	r12

	; mod = in - div*10
	mov	r12, r15
	rla	r15
	rla	r15
	add	r12, r15
	rla	r15
	sub	r15, r14
	
	pop	r8
	pop	r9

	ret

	.data
number:
	.long	1234556789
