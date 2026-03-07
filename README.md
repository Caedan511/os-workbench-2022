# NJU OSLab
目前完成：
amgame
libco
sperf
pmm

## Modify_origin
```
1.AM Makefile:   
	add: -Wno-error=array-bounds -g(line 88,89)  
2.x86_64.mk:   
	origin: export CROSS_COMPILE := x86_64-linux-gnu- (line 1)  
	now:export CROSS_COMPILE :=   
3.qemu.mk:  
	origin:               -machine accel=tcg \(line 5)  
	now:              -machine accel=tcg \  
```

## Environment
```
qemu-system-x86_64: QEMU emulator version 10.1.4 (qemu-10.1.4-1.fc43)
```

## Problem need to be solved  
1.pmm part: The heap memory size is defined directly.   

