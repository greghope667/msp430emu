.DEFAULT_GOAL=build
.PHONY: build run

build:
	make -C asm
	xmake build

run: build
	xmake run msp430emu-cli $(PWD)/asm/code.elf
	
