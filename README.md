## nju oslab
目前完成：
amgame
libco
sperf
pmm

## modify_origin
1.AM Makefile: 
	add: -Wno-error=array-bounds(line 88,89)
2.x86_64.mk: 
	origin: export CROSS_COMPILE := x86_64-linux-gnu- (line 1)
	now:export CROSS_COMPILE := 
3.qemu.mk:
	origin:               -machine accel=tcg \(line 5)
	now:              -machine accel=tcg \
4.kernel.h:
	add:typedef struct cpu cpu_t;(line 30)

