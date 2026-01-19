#include <am.h>
#include <klib.h>
#include <klib-macros.h>
#include <stdarg.h>


#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

char *convert(unsigned long int num, int base);
static inline void puts(const char *s) {
  for (; *s; s++) putch(*s);
}
int printf(const char *fmt, ...) {

  const char *traverse; 
  unsigned int i; 
  char *s; 
  void* p;
  uintptr_t v;
  //Module 1: Initializing Myprintf's arguments 
  va_list arg; 
  va_start(arg, fmt); 

  for(traverse = fmt; *traverse != '\0'; traverse++) 
  { 
    while( *traverse != '%' ) 
    { 
      if( *traverse == '\0') return 0; 
      putch(*traverse);
      traverse++; 
    }

    traverse++; 

    //Module 2: Fetching and executing arguments
    switch(*traverse) 
    { 
      case 'c' : i = va_arg(arg,int);     //Fetch char argument
                  putch(i);
                  break; 

      case 'd' : i = va_arg(arg,int);         //Fetch Decimal/Integer argument
                  if(i<0) 
                  { 
                      i = -i;
                      putch('-'); 
                  } 
                  puts(convert(i,10));
                  break; 

      case 'o': i = va_arg(arg,unsigned int); //Fetch Octal representation
                  puts(convert(i,8));
                  break; 

      case 's': s = va_arg(arg,char *);       //Fetch string
                  puts(s); 
                  break; 

      case 'x': i = va_arg(arg,unsigned int); //Fetch Hexadecimal representation
                  puts(convert(i,16));
                  break; 
      case 'p':  p = va_arg(arg,void*);         //Fetch Decimal/Integer argument
                  v = (uintptr_t)p;
                  puts("0x");
                  puts(convert(v,16));
                  break; 
    }   
  } 

  //Module 3: Closing argument list to necessary clean-up
  va_end(arg); 
  return 0;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  panic("Not implemented");
}

int sprintf(char *out, const char *fmt, ...) {
  panic("Not implemented");
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  panic("Not implemented");
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  panic("Not implemented");
}

char *convert(unsigned long int num, int base) 
{ 
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
