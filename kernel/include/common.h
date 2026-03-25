#include <kernel.h>
#include <klib.h>
#include <klib-macros.h>
#include <limits.h>

#define MAX_CPU 8

//use LOG to printf alloc free message 
// #define PMM_LOG


//test the alloc and free in os_run
// #define PMM_TEST

#define KMT_TEST