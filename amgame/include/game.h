#include <am.h>
#include <amdev.h>
#include <klib.h>
#include <klib-macros.h>

void splash();
void print_key();
void draw_cube();
void image_init();
static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}

//fixed-point number
#define ROOT3(x) (x * 174 + 100 - 1)/100 

typedef int32_t fixed;

#define FP_SHIFT 16
#define FP_ONE   (1 << FP_SHIFT)

#define F(x)     ((fixed)((x) << FP_SHIFT))
#define TO_INT(x) ((x) >> FP_SHIFT)

static inline fixed f_mul(fixed a, fixed b) {
  return (fixed)(((int64_t)a * b) >> FP_SHIFT);
}
static inline fixed f_div(fixed a, fixed b) {
  return (fixed)(((int64_t)a << FP_SHIFT) / b);
}

typedef struct {
  fixed x, y, z;
} vec3;

typedef struct {
  int x, y;
} vec2i;

extern fixed sin_table[360];


//time
static inline uint64_t uptime_us() {
  AM_TIMER_UPTIME_T t;
  ioe_read(AM_TIMER_UPTIME, &t);
  return t.us;
}
