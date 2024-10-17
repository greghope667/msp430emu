PREFIX=msp430-

AS=$(PREFIX)as
LD=$(PREFIX)ld
OBJCOPY=$(PREFIX)objcopy
OBJDUMP=$(PREFIX)objdump

.DEFAULT_GOAL=build

%.o: %.S
	cpp $< | $(AS) -mcpu=430 -o $@

%.elf: link.ld start.o %.o
	$(LD) -T $^ -o $@

%.bin: %.elf
	$(OBJCOPY) -O binary -j .text -j .rodata -j .data $< $@

%.tc: %.bin
	xxd -g1 -c2 $< | awk '{print "0x"$$2 " 0x"$$3}' > $@

build: code.elf u32toa.elf divmod10.elf
