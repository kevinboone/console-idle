/*============================================================================

  bitmaprgb.c

  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <pthread.h>
#include <stdint.h>
#include <stdarg.h>
#include <errno.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <klib/defs.h>
#include <klib/klog.h>
#include <klib/framebuffer.h>
#include <klib/bitmaprgb.h> 

// Bytes per pixel
#define BPP 3

struct _BitmapRGB
  {
  int w;
  int h;
  BYTE *data;
  }; 

#define KLOG_CLASS "klib.bitmaprgb"

/*==========================================================================
  
  math helper functions

  These are all used by the line anti-aliasing functions. I have defined
  them as 'static inline' in the hope that gcc will do the right thing
  and write them straight into the code
    
*==========================================================================*/
static inline float absolute (float x)
  {
  if (x < 0) 
    return -x;
  else
    return x;
  }

static inline void swap (int *x, int *y)
  {
  int t = *y;
  *y = *x;
  *x = t;
  }

static inline float ipart (float x) 
  { 
  return floor (x); 
  } 
  
static inline float rnd (float x) 
  { 
  return ipart (x + 0.5);
  } 
  
static inline float fpart (float x) 
  { 
  return x - floor (x);
  } 
  
static inline float rfpart (float x) 
  { 
  return 1 - fpart (x); 
  } 
  


/*==========================================================================
  bitmaprgb_create
*==========================================================================*/
BitmapRGB *bitmaprgb_create (int w, int h)
  {
  KLOG_IN
  BitmapRGB *self = malloc (sizeof (BitmapRGB));
  self->w = w;
  self->h = h;
  self->data = malloc (w * h * BPP);
  memset (self->data, 0, w * h * BPP);
  KLOG_OUT 
  return self;
  }

/*==========================================================================
  bitmaprgb_create_from_buff
*==========================================================================*/
BitmapRGB *bitmaprgb_create_from_buff (int w, int h, const BYTE *buff)
  {
  KLOG_IN
  BitmapRGB *self = malloc (sizeof (BitmapRGB));
  self->w = w;
  self->h = h;
  self->data = malloc (w * h * BPP);
  memcpy (self->data, buff, w * h * BPP);
  KLOG_OUT 
  return self;
  }

/*==========================================================================
  bitmaprgb_copy_from
*==========================================================================*/
void bitmaprgb_copy_from (BitmapRGB *self, const BitmapRGB *other)
  {
  KLOG_IN
  assert (self->w == other->w);
  assert (self->h == other->h);

  int size = self->w * self->h * BPP;
  memcpy (self->data, other->data, size); 
 
  KLOG_OUT
  }

/*==========================================================================
  bitmaprgb_clone
*==========================================================================*/
BitmapRGB *bitmaprgb_clone (const BitmapRGB *other)
  {
  KLOG_IN
  BitmapRGB *self = bitmaprgb_create (other->w, other->h);

  int size = self->w * self->h * BPP;
  memcpy (self->data, other->data, size); 
 
  KLOG_OUT
  return self;
  }

/*==========================================================================
  bitmaprgb_set_pixel
*==========================================================================*/
void bitmaprgb_set_pixel (BitmapRGB *self, int x, int y, 
      BYTE r, BYTE g, BYTE b)
  {
  if (x >= 0 && x < self->w && y >= 0 && y < self->h)
    {
    int index24 = (y * self->w + x) * BPP;
    self->data [index24++] = b;
    self->data [index24++] = g;
    self->data [index24] = r;
    }
  }

/*==========================================================================
  bitmaprgb_get_pixel
*==========================================================================*/
void bitmaprgb_get_pixel (BitmapRGB *self, int x, int y, 
      BYTE *r, BYTE *g, BYTE *b)
  {
  if (x >= 0 && x < self->w && y >= 0 && y < self->h)
    {
    int index24 = (y * self->w + x) * BPP;
    *b = self->data [index24++];
    *g = self->data [index24++];
    *r = self->data [index24];
    }
  else
    {
    *r = 0; *g = 0; *b = 0;
    }
  }

/*==========================================================================
  bitmaprgb_set_pixel_t

  Set a pixel with a specific 'brightness'. This is just a helper to
  void reproducing all the per-channel brightness code repeatedly 
  elsewhere
*==========================================================================*/
void bitmaprgb_set_pixel_t (BitmapRGB *self, int x, int y, 
      BYTE r, BYTE g, BYTE b, float t)
  {
  b = (BYTE) (t * (float)b);
  g = (BYTE) (t * (float)g);
  r = (BYTE) (t * (float)r);
  if (x > 0 && x < self->w && y > 0 && y < self->h)
    {
    int index24 = (y * self->w + x) * BPP;
    self->data [index24 + 0] = b;
    self->data [index24 + 1] = g;
    self->data [index24 + 2] = r;
    }
  }


/*==========================================================================
  bitmaprgb_fill_rect
  x2,y2 point is _excluded_
*==========================================================================*/
void bitmaprgb_fill_rect (BitmapRGB *self, int x1, int y1,
      int x2, int y2, BYTE r, BYTE g, BYTE b)
  {
  KLOG_IN
  if (x1 > x2) { int t = x1; x2 = x1; x1 = t; }
  if (y1 > y2) { int t = y1; y2 = y1; y1 = t; }
  for (int y = y1; y < y2; y++)
    {
    for (int x = x1; x < x2; x++)
      {
      bitmaprgb_set_pixel (self, x, y, r, g, b);
      }
    }
  KLOG_OUT
  }

/*==========================================================================
  bitmaprgb_destroy
*==========================================================================*/
void bitmaprgb_destroy (BitmapRGB *self)
  {
  KLOG_IN
  if (self)
    {
    if (self->data) free (self->data);
    free (self); 
    }
  KLOG_OUT
  }


/*==========================================================================
  bitmaprgb_to_fb
*==========================================================================*/
void bitmaprgb_to_fb (const BitmapRGB *self, FrameBuffer *fb, int x1, int y1)
  {
  KLOG_IN
  int w_in = self->w;
  int h_in = self->h;
  BYTE *data = framebuffer_get_data (fb);
  int w_out = framebuffer_get_width (fb);
  int h_out = framebuffer_get_height (fb);
  for (int y = 0; y < h_in && y + y1 < h_out; y++)
    {
    int xp = x1;
    int linestart24 = y * w_in;
    int linestart32 = (y + y1) * w_out;
    for (int x = 0; x < w_in && x + x1 < w_out; x++)
      {
      int index24 = (linestart24 + x) * BPP;
      int index32 = (linestart32 + x + x1) * 4;
      BYTE b = self->data [index24++];
      BYTE g = self->data [index24++];
      BYTE r = self->data [index24];
      // Be aware that the initial x,y offset can both be negative. We
      //  don't want to end up reading from before the start of data
      if (index32 >= 0)
        {
        data [index32] = b;
        data [index32+1] = g;
        data [index32+2] = r;
        }
      xp++;
      }
    }
  KLOG_OUT
  }


/*==========================================================================

  bitmaprgb_from_fb

  The bitmaprgb should already be intialized, and have the desired
  sizes

*==========================================================================*/
void bitmaprgb_from_fb (BitmapRGB *self, const FrameBuffer *fb, int x1, int y1)
  {
  KLOG_IN
  int w_in = self->w;
  int h_in = self->h;
  for (int y = 0; y < h_in; y++)
    {
    int yp = y + y1;
    int xp = x1;
    int linestart = y * w_in;
    for (int x = 0; x < w_in; x++)
      {
      BYTE r, g, b;
      framebuffer_get_pixel (fb, xp, yp, &r, &g, &b);
      int index24 = (linestart + x) * BPP;
      self->data [index24++] = b;
      self->data [index24++] = g;
      self->data [index24] = r;
      xp++;
      }
    }
  KLOG_OUT
  }

/*==========================================================================

  bitmaprgb_darken

  Darken to the specified percentage of original value

*==========================================================================*/
void bitmaprgb_darken (BitmapRGB *self, int percent)
  {
  KLOG_IN
  int l = self->w * self->h * BPP;
  for (int i = 0; i < l; i++)
    {
    self->data[i] = self->data[i] * percent / 100;
    }
  KLOG_OUT
  }

/*==========================================================================
  bitmaprgb_get_width
*==========================================================================*/
int bitmaprgb_get_width (const BitmapRGB *self)
  {
  return self->w;
  }

/*==========================================================================
  bitmaprgb_get_height
*==========================================================================*/
int bitmaprgb_get_height (const BitmapRGB *self)
  {
  return self->h;
  }


/*==========================================================================
  
  bitmaprgb_draw_line_one_pixel
  https://en.wikipedia.org/wiki/Xiaolin_Wu%27s_line_algorithm

*==========================================================================*/
#ifdef NOT_INCLUDED
void bitmaprgb_draw_line_one_pixel (BitmapRGB *self, int x0, int y0, 
          int x1, int y1, BYTE r, BYTE g, BYTE b)
  {
  KLOG_IN

  BOOL steep = absolute (y1 - y0) > absolute (x1 - x0); 
  
  if (steep) 
    { 
    swap (&x0, &y0); 
    swap (&x1, &y1); 
    } 
  if (x0 > x1) 
    { 
    swap (&x0, &x1); 
    swap (&y0, &y1); 
    } 
  
  float dx = x1 - x0; 
  float dy = y1 - y0; 
  float gradient = dy / dx; 
  if (dx == 0.0) 
    gradient = 1; 

  float xend = rnd (x0);
  float yend = y0 + gradient * (xend - x0);
  float xgap = rfpart (x0 + 0.5);
  int xpxl1 = xend;
  int ypxl1 = ipart (yend);
  if (steep)
    {
    bitmaprgb_set_pixel_t (self, ypxl1, xpxl1, r, g, b, rfpart (yend) * xgap);
    bitmaprgb_set_pixel_t (self, ypxl1 + 1, xpxl1, r, g, b, fpart(yend) * xgap); 
    }
  else
    {
    bitmaprgb_set_pixel_t (self, xpxl1, ypxl1, r, g, b, rfpart(yend) * xgap);
    bitmaprgb_set_pixel_t (self, xpxl1, ypxl1 + 1, r, g, b, fpart(yend) * xgap);
    }

  float intersectY = yend + gradient; 

  xend = rnd (x1);
  yend = y1 + gradient * (xend - x1);
  xgap = rfpart (x1 + 0.5);
  int xpxl2 = xend;
  int ypxl2 = ipart (yend);
  if (steep)
    {
    bitmaprgb_set_pixel_t (self, ypxl2, xpxl2, r, g, b, rfpart (yend) * xgap);
    bitmaprgb_set_pixel_t (self, ypxl2 + 1, xpxl2, r, g, b, fpart(yend) * xgap); 
    }
  else
    {
    bitmaprgb_set_pixel_t (self, xpxl2, ypxl2, r, g, b, rfpart(yend) * xgap);
    bitmaprgb_set_pixel_t (self, xpxl2, ypxl2 + 1, r, g, b, fpart(yend) * xgap);
    }

   if (steep) 
    { 
    for (int x = xpxl1 ; x <=xpxl2 ; x++) 
      { 
      float t1 = rfpart (intersectY);
      float t2 = fpart (intersectY);
      bitmaprgb_set_pixel_t (self, ipart (intersectY), x, r, g, b, t1); 
      bitmaprgb_set_pixel_t (self, ipart (intersectY) + 1, x, r, g, b, t2);
      intersectY += gradient; 
      } 
    } 
  else
    {
    for (int x = xpxl1 ; x <=xpxl2 ; x++) 
      { 
      float t1 = rfpart (intersectY);
      float t2 = fpart (intersectY);
      bitmaprgb_set_pixel_t (self, x, ipart (intersectY), r, g, b, t1); 
      bitmaprgb_set_pixel_t (self, x, ipart (intersectY) + 1, r, g, b, t2);
      intersectY += gradient; 
      }
    }
  KLOG_OUT
  }
#endif

/*==========================================================================

  bitmaprgb_clear

*==========================================================================*/
void bitmaprgb_clear (BitmapRGB *self, BYTE r, BYTE g, BYTE b) 
  {
  KLOG_IN
  bitmaprgb_fill_rect (self, 0, 0, self->w - 1, self->h - 1, r, g, b); 
  KLOG_OUT
  }

