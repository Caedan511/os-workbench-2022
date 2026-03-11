# NJU OSLab
目前完成：
amgame
libco
sperf
pmm

## Modify_origin
```
1.AM Makefile:   
	add: 	 	-Wno-error=array-bounds -g(line 88,89)  
2.x86_64.mk:   
	origin: 	export CROSS_COMPILE := x86_64-linux-gnu- (line 1)  
	now:		export CROSS_COMPILE :=   
3.qemu.mk:  
	origin:		-machine accel=tcg \(line 5)  
	now:        -machine pc-i440fx-6.0,accel=tcg \
4.am/src/x86/qemu/boot/main.c
	origin:  	Elf32_Ehdr *elf32 = (void *)0x8000; (line 66)
 			 	Elf64_Ehdr *elf64 = (void *)0x8000;
	now:	 	volatile Elf32_Ehdr *elf32 = (void *)0x8000;
 			 	volatile Elf64_Ehdr *elf64 = (void *)0x8000;
```

## Environment
```
qemu-system-x86_64: QEMU emulator version 10.1.4 (qemu-10.1.4-1.fc43)
gcc (GCC) 15.2.1 20260123 (Red Hat 15.2.1-7)
```
## Attention

## Problem need to be solved  
1.pmm part: The heap memory size is defined directly.   

