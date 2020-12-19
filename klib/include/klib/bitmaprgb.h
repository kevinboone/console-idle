/*============================================================================

  bitmaprgb.h
  Copyright (c)2020 Kevin Boone, GPL v3.0

============================================================================*/

#pragma once

#include <klib/defs.h>
#include <klib/framebuffer.h>

struct _BitmapRGB;
typedef struct _BitmapRGB BitmapRGB;

BEGIN_DECLS

BitmapRGB   *bitmaprgb_create (int w, int h);
// Create from an existing memory buffer, which is copied locally
BitmapRGB   *bitmaprgb_create_from_buff (int w, int h, const BYTE *buff);
void         bitmaprgb_destroy (BitmapRGB *self);

void         bitmaprgb_clear (BitmapRGB *self, BYTE r, BYTE g, BYTE b);
void         bitmaprgb_set_pixel (BitmapRGB *self, int x, int y, 
                BYTE r, BYTE g, BYTE b);
void         bitmaprgb_fill_rect (BitmapRGB *self, int x1, int y1,
                int x2, int y2, BYTE r, BYTE g, BYTE b);

/** Copy this bitmap to the framebuffer, starting at offset x,y */
void         bitmaprgb_to_fb (const BitmapRGB *r, FrameBuffer *fb, 
               int x, int y);

/** Copy this bitmap from the framebuffer, starting at offset x,y */
void         bitmaprgb_from_fb (BitmapRGB *self, const FrameBuffer *fb, 
               int x, int y);
void         bitmaprgb_darken (BitmapRGB *self, int percent);
BitmapRGB   *bitmaprgb_clone (const BitmapRGB *other);
int          bitmaprgb_get_height (const BitmapRGB *self);
int          bitmaprgb_get_width (const BitmapRGB *self);
void         bitmaprgb_draw_line_one_pixel (BitmapRGB *self, int x1, int x2, 
                int y1, int y2, BYTE r, BYTE g, BYTE b);
void         bitmaprgb_copy_from (BitmapRGB *self, const BitmapRGB *other);


void         bitmaprgb_get_pixel (BitmapRGB *self, int x, int y, 
                BYTE *r, BYTE *g, BYTE *b);

END_DECLS


