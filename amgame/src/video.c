#include <game.h>

#define EDGE_LEN 80

#define QUICK_CLEAR 20

// #define CLEAR_ADN_DRAWLINE
#ifndef CLEAR_ADN_DRAWLINE
#define USE_BUFFER
#endif

// #define ORGIN

static int w, h;

static void init() {
  AM_GPU_CONFIG_T info = {0};
  ioe_read(AM_GPU_CONFIG, &info);
  w = info.width;
  h = info.height;
}

#ifdef ORGIN
#define SIDE 16
static void draw_tile(int x, int y, int w, int h, uint32_t color) {
  uint32_t pixels[w * h]; // WARNING: large stack-allocated memory
  AM_GPU_FBDRAW_T event = {
    .x = x, .y = y, .w = w, .h = h, .sync = 1,
    .pixels = pixels,
  };
  for (int i = 0; i < w * h; i++) {
    pixels[i] = color;
  }
  ioe_write(AM_GPU_FBDRAW, &event);
}

void splash() {
  init();
  for (int x = 0; x * SIDE <= w; x ++) {
    for (int y = 0; y * SIDE <= h; y++) {
      if ((x & 1) ^ (y & 1)) {
        draw_tile(x * SIDE, y * SIDE, SIDE, SIDE, 0xffffff); // white
      }
    }
  }
}
#endif

#ifdef CLEAR_ADN_DRAWLINE
static void draw_pixel(int x, int y, uint32_t color){
  AM_GPU_FBDRAW_T event = {
    .x = x, .y = y, .w = 1, .h = 1, .sync = true,
    .pixels = &color,
  };
  ioe_write(AM_GPU_FBDRAW, &event);
}

void clear_screen(uint32_t color){
  uint32_t pixels[QUICK_CLEAR * QUICK_CLEAR] = {color};
  AM_GPU_FBDRAW_T event = {
    .w = QUICK_CLEAR, .h = QUICK_CLEAR, .sync = true,
    .pixels = pixels,
  };
  for (int x = 7; x * QUICK_CLEAR <= w-150; x++) {
    for (int y = 3; y * QUICK_CLEAR <= h-80; y++) {
      event.x = x*QUICK_CLEAR;
      event.y = y*QUICK_CLEAR;
      ioe_write(AM_GPU_FBDRAW, &event);
    }
  }
}

#endif

/* 
  spin cube part 
*/
static vec3 cube_vertices[8] = {
  { F(-EDGE_LEN), F(-EDGE_LEN), F(-EDGE_LEN) },
  { F( EDGE_LEN), F(-EDGE_LEN), F(-EDGE_LEN) },
  { F( EDGE_LEN), F( EDGE_LEN), F(-EDGE_LEN) },
  { F(-EDGE_LEN), F( EDGE_LEN), F(-EDGE_LEN) },
  { F(-EDGE_LEN), F(-EDGE_LEN), F( EDGE_LEN) },
  { F( EDGE_LEN), F(-EDGE_LEN), F( EDGE_LEN) },
  { F( EDGE_LEN), F( EDGE_LEN), F( EDGE_LEN) },
  { F(-EDGE_LEN), F( EDGE_LEN), F( EDGE_LEN) },
};

static int cube_edges[12][2] = {
  {0,1},{1,2},{2,3},{3,0},
  {4,5},{5,6},{6,7},{7,4},
  {0,4},{1,5},{2,6},{3,7}
};

#ifdef USE_BUFFER
static int32_t buffer[ROOT3(EDGE_LEN)*2][ROOT3(EDGE_LEN)*2];

static void update_image(){
  AM_GPU_FBDRAW_T event = {
    .x = w/2 - ROOT3(EDGE_LEN), .y = h/2 - ROOT3(EDGE_LEN), .w = ROOT3(EDGE_LEN)*2, .h = ROOT3(EDGE_LEN)*2, .sync = 1,
    .pixels = buffer,
  };
  ioe_write(AM_GPU_FBDRAW, &event);
  for(int i = 0;i < LENGTH(buffer);i++){
    for(int j = 0;j < LENGTH(buffer[0]);j++){
      buffer[i][j] = 0x3a006f;
    }
  }
}
#endif

void image_init(){
  init();
  printf("the screen size is: %d * %d\n", w, h);
}


static void draw_line(int x0, int y0, int x1, int y1, int color){
  int dx = (x1 > x0) ? (x1 - x0) : (x0 - x1);
  int dy = (y1 > y0) ? (y1 - y0) : (y0 - y1);

  int sx = (x0 < x1) ? 1 : -1;
  int sy = (y0 < y1) ? 1 : -1;

  int err = dx - dy;

  while (1) {

  #ifdef CLEAR_ADN_DRAWLINE
    draw_pixel(x0, y0, color);
  #endif

  #ifdef USE_BUFFER
    buffer[x0 - w/2 + ROOT3(EDGE_LEN)][y0 - h/2 + ROOT3(EDGE_LEN)] = color;
  #endif

    if (x0 == x1 && y0 == y1) break;

    int e2 = err << 1;

    if (e2 > -dy) {
      err -= dy;
      x0 += sx;
    }
    if (e2 < dx) {
      err += dx;
      y0 += sy;
    }
  }
}


static vec3 rotate(vec3 v, int angle) {
  fixed s = sin_table[angle];
  fixed c = sin_table[(angle + 90)%360];

  vec3 r;
  r.x = f_mul(v.x, c) + f_mul(v.z, s);
  r.y = v.y;
  r.z = -f_mul(v.x, s) + f_mul(v.z, c);

  r.y = f_mul(r.y, c) + f_mul(r.z, s);
  r.x = r.x;
  r.z = -f_mul(r.y, s) + f_mul(r.z, c);
  return r;
}

static vec2i project(vec3 v){
  vec2i p;
  p.x = w/2 + TO_INT(v.x);
  p.y = h/2 + TO_INT(v.y);
  return p;
}

void draw_cube(int angle)
{
  vec3 rotated_v[8];
  vec2i project_v[8];
  for(int i = 0;i < 8;i++)
  {
    rotated_v[i] = rotate(cube_vertices[i], angle);
    project_v[i] = project(rotated_v[i]);
  }

#ifdef CLEAR_ADN_DRAWLINE
  clear_screen(0x00ff00);
#endif

  for(int i = 0;i < 12;i++){
    draw_line(project_v[cube_edges[i][0]].x, project_v[cube_edges[i][0]].y,\
              project_v[cube_edges[i][1]].x, project_v[cube_edges[i][1]].y,\
              0xffffff);
  }

#ifdef USE_BUFFER
  update_image();
#endif
}


