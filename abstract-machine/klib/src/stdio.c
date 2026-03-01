#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)


static char *convert(unsigned long int num, int base);

static void emit_string(void (*emit)(char, void *), void *arg, const char *s) {
  while (*s) emit(*s++, arg);
}
//the printf core, printf, all printf class use it 
int vprintf_core(void (*emit)(char, void *), void *arg, 
                const char *fmt, va_list ap) {

  unsigned int i;
  const char *traverse; 

  for(traverse = fmt; *traverse != '\0'; traverse++) { 
    while( *traverse != '%' && *traverse != '\0') { 
      emit(*traverse, arg);
      traverse++; 
    }
    if (*traverse == '\0')
      break;

    traverse++; 

    //Module 2: Fetching and executing arguments
    switch(*traverse) 
    { 
      case 'c': i = va_arg(ap,int);     //Fetch char argument
                  emit(i, arg);
                  break; 

      case 'd': int d = va_arg(ap,int);         //Fetch Decimal/Integer argument
                  if(d < 0) {  
                    emit('-', arg);
                    d = -d;
                  } 
                  emit_string(emit, arg, convert(d, 10));
                  break; 

      case 'o': i = va_arg(ap,unsigned int); //Fetch Octal representation
                  emit_string(emit, arg, convert(i, 8));
                  break; 

      case 's': char *s = va_arg(ap, char *);       //Fetch string
                  emit_string(emit, arg, s); 
                  break; 

      case 'x': i = va_arg(ap,unsigned int); //Fetch Hexadecimal representation
                  emit_string(emit, arg, convert(i, 16));
                  break; 
      case 'p': void *p = va_arg(ap,void*);         //Fetch Decimal/Integer argument
                  uintptr_t v = (uintptr_t)p;
                  emit('0', arg);
                  emit('x', arg);
                  emit_string(emit, arg, convert(v, 16));
                  break; 
    }   
  } 

  return 0;
}

static void console_emit(char c, void *arg) {
  putch(c);
}
int printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int ret = vprintf_core(console_emit, NULL, fmt, ap);

  va_end(ap);
  return ret; 
}


struct buf_ctx {
  char *buf;
  size_t n;
  size_t pos;
};

static void buffer_emit(char c, void *arg) {
  struct buf_ctx *ctx = arg;
  if (ctx->pos + 1 < ctx->n)
      ctx->buf[ctx->pos] = c;
  ctx->pos++;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  struct buf_ctx ctx = {
    .buf = out,
    .n = (size_t)-1,
    .pos = 0,
  };

  vprintf_core(buffer_emit, &ctx, fmt, ap);

  out[ctx.pos] = '\0';
  return ctx.pos;
}



int sprintf(char *out, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  struct buf_ctx ctx = {
    .buf = out,
    .n = (size_t)-1,   
    .pos = 0,
  };

  vprintf_core(buffer_emit, &ctx, fmt, ap);

  out[ctx.pos] = '\0';

  va_end(ap);
  return ctx.pos;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  struct buf_ctx ctx = { 
    .buf = out,
    .n =  n,
    .pos = 0 
  };

  vprintf_core(buffer_emit, &ctx, fmt, ap);

  if (n > 0) {
      if (ctx.pos < n) out[ctx.pos] = '\0';
      else out[n - 1] = '\0';
  }

  va_end(ap);
  return ctx.pos;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  struct buf_ctx ctx = {
    .buf = out,
    .n = n,
    .pos = 0,
  };

  vprintf_core(buffer_emit, &ctx, fmt, ap);

  if (n > 0) {
    if (ctx.pos < n) out[ctx.pos] = '\0';
    else out[n - 1] = '\0';
  }

  return ctx.pos;
}

static char *convert(unsigned long int num, int base) { 
  static char Representation[]= "0123456789ABCDEF";
  static char buffer[50]; 
  char *ptr; 

  ptr = &buffer[49]; 
  *ptr = '\0'; 

  do 
  { 
      *--ptr = Representation[num%base]; 
      num /= base; 
  }while(num != 0); 

  return(ptr); 
}

#endif
