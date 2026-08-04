/* MegaZeux
 *
 * Copyright (C) 2004-2006 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2007 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2007 Alan Williams <mralert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "../render.h"
#include "../renderers.h"

#include "../../graphics.h"
#include "../../util.h"
#include "../../io/image_png.h"
#include "../../io/vio.h"

#include "../../event/3ds/event_3ds.h"
#include "../../event/3ds/keyboard.h"
#include "../../platform/3ds/platform_3ds.h"

#include "renderer_ctr.h"
#include "shader_2d_shbin.h"
#include "shader_playfield_shbin.h"

//#define RDR_DEBUG

struct ctr_shader_data
{
  DVLB_s *dvlb;
  shaderProgram_s program;
  int proj_loc, offs_loc, geo_count;
  C3D_AttrInfo attr;
};

struct ctr_layer
{
  unsigned int w, h, mode;
  float z;
  int draw_order;
  boolean has_background_texture;
  struct v_char *foreground;
  C3D_Tex background;
};

struct v_char
{
  s16 x, y;
  u32 uv;
  u32 col;
};

struct vertex
{
  float x, y, w, h;
  float z;
  float tx, ty, tw, th;
  u32 color;
};

struct linear_ptr_list_entry
{
  void *ptr;
  struct linear_ptr_list_entry *next;
};

#define CTR_TOP_WIDTH (gfxIsWide() ? 800 : 400)
//#define CTR_TOP_HEIGHT 240
#define CTR_CHAR_H 16
#define CTR_TEXTURE_WIDTH 1024
#define CTR_TEXTURE_CHARS_ROW (CTR_TEXTURE_WIDTH / CHAR_W)
#define CTR_TEXTURE_HEIGHT ((FULL_CHARSET_SIZE / CTR_TEXTURE_CHARS_ROW) * CTR_CHAR_H)
#define CTR_LAYER_MAX (LAYER_DRAWORDER_MAX + 101)
#define CTR_LAYER_CURSOR (LAYER_DRAWORDER_MAX + 50)
#define CTR_LAYER_MOUSE (LAYER_DRAWORDER_MAX + 75)

struct ctr_render_data
{
  C3D_Tex charset[5], charset_vram[5];
  u8 charset_dirty_set;
  u8 charset_dirty[NUM_CHARSETS * 2];
  boolean rendering_frame, checked_frame;
  struct ctr_shader_data shader_2d, shader_playfield;
  C3D_Mtx projection;
  C3D_Tex playfield_tex;
  C3D_TexEnv env_normal, env_no_texture, env_playfield, env_playfield_inv;
  C3D_RenderTarget *playfield, *target_top, *target_bottom;
  u8 cursor_on, mouse_on;
  u32 last_focus_x, last_focus_y;
  u32 focus_x, focus_y;
  u32 layer_num;
};

static u8 morton_lut[64] =
{
  0x00, 0x01, 0x04, 0x05, 0x10, 0x11, 0x14, 0x15,
  0x02, 0x03, 0x06, 0x07, 0x12, 0x13, 0x16, 0x17,
  0x08, 0x09, 0x0c, 0x0d, 0x18, 0x19, 0x1c, 0x1d,
  0x0a, 0x0b, 0x0e, 0x0f, 0x1a, 0x1b, 0x1e, 0x1f,
  0x20, 0x21, 0x24, 0x25, 0x30, 0x31, 0x34, 0x35,
  0x22, 0x23, 0x26, 0x27, 0x32, 0x33, 0x36, 0x37,
  0x28, 0x29, 0x2c, 0x2d, 0x38, 0x39, 0x3c, 0x3d,
  0x2a, 0x2b, 0x2e, 0x2f, 0x3a, 0x3b, 0x3e, 0x3f
};

static u8 morton_lut4[32] =
{
  0x00, 0x02, 0x08, 0x0a, 0x01, 0x03, 0x09, 0x0b,
  0x04, 0x06, 0x0c, 0x0e, 0x05, 0x07, 0x0d, 0x0f,
  0x10, 0x12, 0x18, 0x1a, 0x11, 0x13, 0x19, 0x1b,
  0x14, 0x16, 0x1c, 0x1e, 0x15, 0x17, 0x1d, 0x1f
};

// 2 char bits -> 8 texture bits
static u8 bitmask_mzx[4] = { 0x00, 0xf0, 0x0f, 0xff };

// 4 char bits -> 8 texture bits
// bitmask_smzx[layer, 0-3][bits]
static u8 bitmask_smzx[4][16] =
{
  { 0xff, 0x0f, 0x0f, 0x0f, 0xf0, 0x00, 0x00, 0x00,
    0xf0, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00 },
  { 0x00, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0x0f, 0x0f,
    0x00, 0xf0, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00 },
  { 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xf0, 0x00,
    0x0f, 0x0f, 0xff, 0x0f, 0x00, 0x00, 0xf0, 0x00 },
  { 0x00, 0x00, 0x00, 0xf0, 0x00, 0x00, 0x00, 0xf0,
    0x00, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x0f, 0xff }
};

static size_t ctr_png_read_fn(void * RESTRICT dest, size_t count, void * RESTRICT handle)
{
  return vfread(dest, 1, count, (vfile *)handle);
}

static png_bool ctr_png_alloc_fn(uint32_t width, uint32_t height,
 struct png_rgba **pixels, size_t *pixel_row_pitch, void *priv)
{
  C3D_Tex **dest = (C3D_Tex **)priv;
  C3D_Tex *tex;

  // texture PNG dimensions must be powers of two
  if(width < 8 || width > 1024 || height < 8 || height > 1024 ||
   (width & (width - 1)) || (height & (height - 1)))
    return IMAGE_FALSE;

  tex = (C3D_Tex *)ccalloc(1, sizeof(C3D_Tex));

  if(!C3D_TexInit(tex, width, height, GPU_RGBA8))
  {
    free(tex);
    return IMAGE_FALSE;
  }

  *dest = tex;
  *pixels = (struct png_rgba *)tex->data;
  *pixel_row_pitch = width * sizeof(struct png_rgba);
  return IMAGE_TRUE;
}

C3D_Tex *ctr_load_png(const char *name)
{
  C3D_Tex *output = NULL;
  u32 *data;
  u32 *dataBuf;
  u16 width;
  u16 height;
  int i;
  enum png_error ret;

  vfile *vf = vfopen_unsafe(name, "rb");
  if(!vf)
    return NULL;

  ret = png_read(vf, ctr_png_read_fn, &output, ctr_png_alloc_fn, false);
  vfclose(vf);
  if(ret != PNG_OK)
  {
    C3D_TexDelete(output);
    free(output);
    return NULL;
  }

  C3D_TexSetWrap(output, GPU_CLAMP_TO_BORDER, GPU_CLAMP_TO_BORDER);
  data = (u32 *)output->data;
  dataBuf = (u32 *)clinearAlloc(output->size, 0x10);

  for(i = 0; i < output->size >> 2; i++)
    dataBuf[i] = __builtin_bswap32(data[i]);

  width = output->width;
  height = output->height;
  C3D_TexDelete(output);
  C3D_TexInitVRAM(output, width, height, GPU_RGBA8);
  data = (u32 *)output->data;

  GSPGPU_FlushDataCache(dataBuf, output->size);
  C3D_SyncDisplayTransfer(
    dataBuf,
    GX_BUFFER_DIM(output->width, output->height),
    data,
    GX_BUFFER_DIM(output->width, output->height),
    (GX_TRANSFER_FLIP_VERT(0) |
     GX_TRANSFER_OUT_TILED(1) |
     GX_TRANSFER_RAW_COPY(0) |
     GX_TRANSFER_IN_FORMAT(GPU_RGBA8) |
     GX_TRANSFER_OUT_FORMAT(GPU_RGBA8) |
     GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO))
  );

  linearFree(dataBuf);
  return output;
}

static inline void ctr_set_2d_projection(struct ctr_render_data *render_data,
 int width, int height, boolean tilt)
{
  if(tilt)
    Mtx_OrthoTilt(&render_data->projection, 0, width, height, 0, -1.0, CTR_LAYER_MAX,
     true);

  else
    Mtx_Ortho(&render_data->projection, 0, width, 0, height, -1.0, CTR_LAYER_MAX,
     true);
}

static inline void ctr_set_2d_projection_screen(
 struct ctr_render_data *render_data, boolean top_screen)
{
  ctr_set_2d_projection(render_data, top_screen ? 400 : 320, 240, true);
}

static void ctr_bind_shader(struct ctr_shader_data *shader)
{
  C3D_BindProgram(&shader->program);
  C3D_SetAttrInfo(&shader->attr);
}

static inline void ctr_prepare_2d(struct ctr_render_data *render_data,
 struct vertex *array)
{
  C3D_BufInfo *bufInfo;

  bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, array, sizeof(struct vertex), 4, 0x3210);

  ctr_bind_shader(&render_data->shader_2d);
  C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER, render_data->shader_2d.proj_loc,
   &render_data->projection);
}

static inline void ctr_prepare_playfield(struct ctr_render_data *render_data,
 struct v_char *array, float xo, float yo, float z)
{
  C3D_BufInfo *bufInfo;

  bufInfo = C3D_GetBufInfo();
  BufInfo_Init(bufInfo);
  BufInfo_Add(bufInfo, array, sizeof(struct v_char), 3, 0x210);

  ctr_bind_shader(&render_data->shader_playfield);
  C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER, render_data->shader_playfield.proj_loc,
   &render_data->projection);
  C3D_FVUnifSet(GPU_GEOMETRY_SHADER, render_data->shader_playfield.offs_loc,
   (float) xo, (float) yo, z, 0.0f);
}

static int vertex_heap_pos = 0, vertex_heap_len = 0;
static struct vertex *vertex_heap;

void ctr_draw_2d_texture(struct ctr_render_data *render_data, C3D_Tex *texture,
 int tx, int ty, int tw, int th,
 float x, float y, float w, float h, float z, u32 color, boolean flipy)
{
  struct vertex *vertices;

  if(flipy && texture != NULL)
  {
    ty = texture->height - ty;
    th = -th;
  }

  if(vertex_heap_len == 0)
  {
    vertex_heap_len = 64;
    vertex_heap = (struct vertex *)clinearAlloc(sizeof(struct vertex) * vertex_heap_len, 0x80);
  }
  else

  if(vertex_heap_pos + 1 > vertex_heap_len)
  {
    struct vertex *vertex_heap_new =
     (struct vertex *)clinearAlloc(sizeof(struct vertex) * vertex_heap_len * 2, 0x80);
    memcpy(vertex_heap_new, vertex_heap,
     sizeof(struct vertex) * vertex_heap_len);
    vertex_heap_len *= 2;
    linearFree(vertex_heap);
    vertex_heap = vertex_heap_new;
  }

  vertices = &vertex_heap[vertex_heap_pos];
  vertex_heap_pos++;

  vertices[0].x = x;
  vertices[0].y = y;
  vertices[0].w = w;
  vertices[0].h = h;
  vertices[0].z = z;
  vertices[0].color = color;

  ctr_prepare_2d(render_data, vertex_heap);
  if(texture != NULL)
  {
    vertices[0].tx = tx / ((float) texture->width);
    vertices[0].ty = ty / ((float) texture->height);
    vertices[0].tw = tw / ((float) texture->width);
    vertices[0].th = th / ((float) texture->height);

    C3D_SetTexEnv(0, &(render_data->env_normal));
    C3D_TexBind(0, texture);
  }
  else
  {
    vertices[0].tx = tx;
    vertices[0].ty = ty;
    vertices[0].tw = tw;
    vertices[0].th = th;

    C3D_SetTexEnv(0, &(render_data->env_no_texture));
  }
  C3D_DrawArrays(GPU_GEOMETRY_PRIM, vertex_heap_pos - 1, 1);
}

static void ctr_init_shader(struct ctr_shader_data *shader, const void *data,
 int size, int geo_count)
{
  shader->dvlb = DVLB_ParseFile((u32 *)data, size);
  shader->geo_count = geo_count;
  shaderProgramInit(&shader->program);
  shaderProgramSetVsh(&shader->program, &shader->dvlb->DVLE[0]);
  shaderProgramSetGsh(&shader->program, &shader->dvlb->DVLE[1], geo_count);
  shader->proj_loc =
   shaderInstanceGetUniformLocation(shader->program.geometryShader, "projection");
  shader->offs_loc =
   shaderInstanceGetUniformLocation(shader->program.geometryShader, "offset");
  AttrInfo_Init(&shader->attr);
}

static boolean wide_requested = false, wide_new_state;

void ctr_request_set_wide(bool wide)
{
  wide_requested = true;
  wide_new_state = wide;
}

static void ctr_set_wide(struct ctr_render_data *render_data, bool wide)
{
  gfxSetWide(wide);

  C3D_RenderTargetDelete(render_data->target_top);
  render_data->target_top =
   C3D_RenderTargetCreate(240, CTR_TOP_WIDTH, GPU_RB_RGB8, GPU_RB_DEPTH16);
  C3D_RenderTargetClear(render_data->target_top, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetSetOutput(render_data->target_top, GFX_TOP, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
}

static boolean ctr_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  static struct ctr_render_data render_data;
  unsigned int bits_per_pixel = 16;

#ifdef RDR_DEBUG
  consoleInit(GFX_TOP, NULL);
#endif

  memset(&render_data, 0, sizeof(struct ctr_render_data));
  graphics->render_data = &render_data;

  gfxSet3D(false);
  C3D_Init(0x40000);
  C3D_CullFace(GPU_CULL_NONE);

  render_data.rendering_frame = false;
  render_data.checked_frame = false;

  if(conf->force_bpp == 32)
    bits_per_pixel = conf->force_bpp;

  // 1024x512 is the smallest power of two texture which can fit a 640x350 playfield
  if(bits_per_pixel == 32)
    C3D_TexInitVRAM(&render_data.playfield_tex, 1024, 512, GPU_RGBA8);
  else
    C3D_TexInitVRAM(&render_data.playfield_tex, 1024, 512, GPU_RGB565);
  C3D_TexSetFilter(&render_data.playfield_tex, GPU_LINEAR, GPU_LINEAR);

  render_data.playfield =
   C3D_RenderTargetCreateFromTex(&render_data.playfield_tex, GPU_TEXFACE_2D, 0,
    GPU_RB_DEPTH16);
  render_data.target_top =
   C3D_RenderTargetCreate(240, 400, GPU_RB_RGB8, GPU_RB_DEPTH16);
  render_data.target_bottom =
   C3D_RenderTargetCreate(240, 320, GPU_RB_RGB8, GPU_RB_DEPTH16);

  C3D_RenderTargetClear(render_data.playfield, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetClear(render_data.target_top, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetClear(render_data.target_bottom, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_RenderTargetSetOutput(render_data.target_top, GFX_TOP, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));
  C3D_RenderTargetSetOutput(render_data.target_bottom, GFX_BOTTOM, GFX_LEFT,
    GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGB8) |
    GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8));

  ctr_init_shader(&render_data.shader_2d,
   shader_2d_shbin, shader_2d_shbin_size, 4);
  ctr_init_shader(&render_data.shader_playfield,
   shader_playfield_shbin, shader_playfield_shbin_size, 3);

  // v0 = x,y,w,h
  // v1 = z
  // v2 = tx,ty,tw,th
  // v3 = color
  AttrInfo_AddLoader(&render_data.shader_2d.attr, 0, GPU_FLOAT, 4);
  AttrInfo_AddLoader(&render_data.shader_2d.attr, 1, GPU_FLOAT, 1);
  AttrInfo_AddLoader(&render_data.shader_2d.attr, 2, GPU_FLOAT, 4);
  AttrInfo_AddLoader(&render_data.shader_2d.attr, 3, GPU_UNSIGNED_BYTE, 4);

  // v0 = position
  // v1 = texcoord
  // v2 = color
  AttrInfo_AddLoader(&render_data.shader_playfield.attr, 0, GPU_SHORT, 2);
  AttrInfo_AddLoader(&render_data.shader_playfield.attr, 1, GPU_SHORT, 2);
  AttrInfo_AddLoader(&render_data.shader_playfield.attr, 2, GPU_UNSIGNED_BYTE, 4);

  for(int i = 0; i < 5; i++)
  {
    int tex_width = CTR_TEXTURE_WIDTH;
    if(i > 0)
      tex_width /= 2;

    C3D_TexInit(&render_data.charset[i], tex_width,
      CTR_TEXTURE_HEIGHT, GPU_A4);
    C3D_TexSetFilter(&render_data.charset[i], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&render_data.charset[i], GPU_REPEAT, GPU_REPEAT);

    C3D_TexInitVRAM(&render_data.charset_vram[i], tex_width,
      CTR_TEXTURE_HEIGHT, GPU_A4);
    C3D_TexSetFilter(&render_data.charset_vram[i], GPU_NEAREST, GPU_NEAREST);
    C3D_TexSetWrap(&render_data.charset_vram[i], GPU_REPEAT, GPU_REPEAT);
  }

  C3D_TexEnvInit(&(render_data.env_normal));
  C3D_TexEnvSrc(&(render_data.env_normal), C3D_Both, GPU_TEXTURE0, (GPU_TEVSRC) 0, (GPU_TEVSRC) 0);
  C3D_TexEnvOpRgb(&(render_data.env_normal), (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0);
  C3D_TexEnvOpAlpha(&(render_data.env_normal), (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0);
  C3D_TexEnvFunc(&(render_data.env_normal), C3D_Both, GPU_MODULATE);

  C3D_TexEnvInit(&(render_data.env_no_texture));
  C3D_TexEnvSrc(&(render_data.env_no_texture), C3D_Both, GPU_PRIMARY_COLOR, (GPU_TEVSRC) 0, (GPU_TEVSRC) 0);
  C3D_TexEnvOpRgb(&(render_data.env_no_texture), (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0, (GPU_TEVOP_RGB) 0);
  C3D_TexEnvOpAlpha(&(render_data.env_no_texture), (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0, (GPU_TEVOP_A) 0);
  C3D_TexEnvFunc(&(render_data.env_no_texture), C3D_Both, GPU_REPLACE);

  C3D_TexEnvInit(&(render_data.env_playfield));
  C3D_TexEnvSrc(&(render_data.env_playfield), C3D_RGB, (GPU_TEVSRC) 0, GPU_PRIMARY_COLOR, (GPU_TEVSRC) 0);
  C3D_TexEnvSrc(&(render_data.env_playfield), C3D_Alpha, (GPU_TEVSRC) 0, GPU_TEXTURE0, (GPU_TEVSRC) 0);
  C3D_TexEnvOpRgb(&(render_data.env_playfield), (GPU_TEVOP_RGB) 0, GPU_TEVOP_RGB_SRC_ALPHA, (GPU_TEVOP_RGB) 0);
  C3D_TexEnvOpAlpha(&(render_data.env_playfield), (GPU_TEVOP_A) 0, GPU_TEVOP_A_SRC_ALPHA, (GPU_TEVOP_A) 0);
  C3D_TexEnvFunc(&(render_data.env_playfield), C3D_Both, GPU_MODULATE);

  C3D_TexEnvInit(&(render_data.env_playfield_inv));
  C3D_TexEnvSrc(&(render_data.env_playfield_inv), C3D_RGB, (GPU_TEVSRC) 0, GPU_PRIMARY_COLOR, (GPU_TEVSRC) 0);
  C3D_TexEnvSrc(&(render_data.env_playfield_inv), C3D_Alpha, (GPU_TEVSRC) 0, GPU_TEXTURE0, (GPU_TEVSRC) 0);
  C3D_TexEnvOpRgb(&(render_data.env_playfield_inv), (GPU_TEVOP_RGB) 0, GPU_TEVOP_RGB_SRC_ALPHA, (GPU_TEVOP_RGB) 0);
  C3D_TexEnvOpAlpha(&(render_data.env_playfield_inv), (GPU_TEVOP_A) 0, GPU_TEVOP_A_ONE_MINUS_SRC_ALPHA, (GPU_TEVOP_A) 0);
  C3D_TexEnvFunc(&(render_data.env_playfield_inv), C3D_Both, GPU_MODULATE);

  C3D_AlphaTest(false, GPU_GREATER, 0x80);
  C3D_DepthTest(false, GPU_GEQUAL, GPU_WRITE_ALL);

  graphics->allow_resize = 0;
  graphics->bits_per_pixel = bits_per_pixel;

  graphics->resolution_width = 640;
  graphics->resolution_height = 350;
  graphics->window_width = 640;
  graphics->window_height = 350;

  ctr_keyboard_init(&render_data);
  return true;
}

static void ctr_free_video(struct graphics_data *graphics)
{
  // TODO: more freeing!
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  int i;

  for(i = 0; i < 5; i++)
  {
    C3D_TexDelete(&render_data->charset[i]);
    C3D_TexDelete(&render_data->charset_vram[i]);
  }
  C3D_TexDelete(&render_data->playfield_tex);

  C3D_RenderTargetDelete(render_data->playfield);
  C3D_RenderTargetDelete(render_data->target_top);
  C3D_RenderTargetDelete(render_data->target_bottom);

  C3D_Fini();
}

static boolean ctr_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  return true;
}

static void ctr_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  unsigned int i;

  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] =
     (palette[i].r << 24) | (palette[i].g << 16) |
     (palette[i].b <<  8) | 0x000000FF;
  }
}

static inline u32 ctr_get_char_texture_row(u32 chr)
{
  return chr / CTR_TEXTURE_CHARS_ROW;
}

/**
 * This method takes a single character line "c" for character "chr" and
 * Y position "y" and writes it to all textures (both MZX and SMZX).
 */
static inline void ctr_char_line_to_texture(
 struct ctr_render_data *render_data, u8 c, u32 chr, int y)
{
  u8 *p0, *p1, *p2, *p3;
  u8 ch_low, ch_high;
  u32 offset = ((chr % CTR_TEXTURE_CHARS_ROW) * (8 * 8 / 2));
  u32 offset_smzx = (((chr & (~1)) % CTR_TEXTURE_CHARS_ROW) * (4 * 8 / 2));

  offset += (chr / CTR_TEXTURE_CHARS_ROW) * (CTR_TEXTURE_WIDTH * CTR_CHAR_H / 2);
  offset_smzx += (chr / CTR_TEXTURE_CHARS_ROW) * (CTR_TEXTURE_WIDTH / 2 * CTR_CHAR_H / 2);

  // Add the Y position to the offsets, taking into account the 8x8 swizzling
  offset += ((y / 8) * (CTR_TEXTURE_WIDTH * 8 / 2));
  offset += morton_lut4[(y & 7) * 4];

  offset_smzx += ((y / 8) * (CTR_TEXTURE_WIDTH / 2 * 8 / 2));
  offset_smzx += morton_lut4[(y & 7) * 4];
  if(chr & 1)
    offset_smzx += 8;

  // Write line to MZX texture, taking into account the swizzling
  p0 = ((u8 *)render_data->charset[0].data) + offset;
  p0[0]  = bitmask_mzx[(c >> 6)       ];
  p0[2]  = bitmask_mzx[(c >> 4) & 0x03];
  p0[8]  = bitmask_mzx[(c >> 2) & 0x03];
  p0[10] = bitmask_mzx[(c     ) & 0x03];

  // Write line to SMZX texture, taking into account the swizzling
  p0 = ((u8 *)render_data->charset[1].data) + offset_smzx;
  p1 = ((u8 *)render_data->charset[2].data) + offset_smzx;
  p2 = ((u8 *)render_data->charset[3].data) + offset_smzx;
  p3 = ((u8 *)render_data->charset[4].data) + offset_smzx;

  ch_high = (c >> 4);
  ch_low = (c & 15);

  p0[0] = bitmask_smzx[0][ch_high];
  p0[2] = bitmask_smzx[0][ch_low];
  p1[0] = bitmask_smzx[1][ch_high];
  p1[2] = bitmask_smzx[1][ch_low];
  p2[0] = bitmask_smzx[2][ch_high];
  p2[2] = bitmask_smzx[2][ch_low];
  p3[0] = bitmask_smzx[3][ch_high];
  p3[2] = bitmask_smzx[3][ch_low];
}

static void ctr_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  u16 end = first + count;
  u8 *charset_pos;
  u32 i, j;

  if(end > FULL_CHARSET_SIZE)
    end = FULL_CHARSET_SIZE;

  charset_pos = graphics->charset;
  charset_pos += first * CHAR_SIZE;

  for(i = first; i < end; i++)
  {
    for(j = 0; j < 14; j++, charset_pos++)
      ctr_char_line_to_texture(render_data, *charset_pos, i, j);
  }

  first = ctr_get_char_texture_row(first);
  end = ctr_get_char_texture_row(end - 1);

  for(i = first; i <= end; i++)
    render_data->charset_dirty[i] = 1;
  render_data->charset_dirty_set = 1;
}

static void ctr_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  u16 tex_row = ctr_get_char_texture_row(chr);
  u8 *charset_pos;
  u32 i;

  charset_pos = graphics->charset;
  charset_pos += chr * CHAR_SIZE;

  for(i = 0; i < 14; i++, charset_pos++)
    ctr_char_line_to_texture(render_data, *charset_pos, chr, i);

  render_data->charset_dirty[tex_row] = 1;
  render_data->charset_dirty_set = 1;
}

static void ctr_remap_charbyte(struct graphics_data *graphics, uint16_t chr,
 uint8_t byte)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  u16 tex_row = ctr_get_char_texture_row(chr);
  u8 *charset_pos;

  charset_pos = graphics->charset;
  charset_pos += chr * CHAR_SIZE + byte;

  ctr_char_line_to_texture(render_data, *charset_pos, chr, byte);

  render_data->charset_dirty[tex_row] = 1;
  render_data->charset_dirty_set = 1;
}

static inline void ctr_refresh_charsets(struct ctr_render_data *render_data,
 int from, int to)
{
  u8 *charset_dirty;
  int coffs = 0;
  int csize = 0;
  int cincr = (CTR_TEXTURE_WIDTH * CTR_CHAR_H) / 2;
  int last_dirty = 0;
  int i;

  if(!render_data->charset_dirty_set)
    return;

  render_data->charset_dirty_set = 0;
  charset_dirty = render_data->charset_dirty;

  for(i = 0; i < NUM_CHARSETS * 2; i++)
  {
    if(charset_dirty[i])
    {
      coffs = cincr * i;
      break;
    }
  }

  for(; i < NUM_CHARSETS * 2; i++)
  {
    if(charset_dirty[i])
    {
      charset_dirty[i] = 0;
      last_dirty = i;
    }
  }

  csize = (last_dirty + 1) * cincr - coffs;
  if(csize <= 0)
    return;

  for(i = from; i < to; i++)
  {
    if(i != 0)
    {
      GSPGPU_FlushDataCache((u8 *)(render_data->charset[i].data) + coffs / 2, csize / 2);
      C3D_SyncTextureCopy((u32 *)((u8 *)(render_data->charset[i].data) + coffs / 2), 0,
        (u32 *)((u8 *)(render_data->charset_vram[i].data) + coffs / 2), 0, csize / 2, 8);
    }
    else
    {
      GSPGPU_FlushDataCache((u8 *)(render_data->charset[0].data) + coffs, csize);
      C3D_SyncTextureCopy((u32 *)((u8 *)(render_data->charset[0].data) + coffs), 0,
        (u32 *)((u8 *)(render_data->charset_vram[0].data) + coffs), 0, csize, 8);
    }
  }
  return;
}

static boolean ctr_should_render(struct ctr_render_data *render_data)
{
  if(!render_data->rendering_frame)
  {
    // If we aren't currently in control of the GPU, do not draw video.
    if(!gspHasGpuRight())
      return false;

    if(wide_requested)
    {
      ctr_set_wide(render_data, wide_new_state);
      wide_requested = false;
    }

    if(render_data->checked_frame || !C3D_FrameBegin(C3D_FRAME_NONBLOCK))
    {
      render_data->checked_frame = true;
      return false;
    }
    ctr_refresh_charsets(render_data, 0, 5);
    render_data->rendering_frame = true;
    render_data->layer_num = 0;
    vertex_heap_pos = 0;
    C3D_FrameDrawOn(render_data->playfield);
    ctr_set_2d_projection(render_data, 1024, 512, false);
  }
  return true;
}

static inline u32 ctr_char_texture_uv(u32 ch)
{
  u16 u = (ch % CTR_TEXTURE_CHARS_ROW) * CHAR_W;
  u16 v = CTR_TEXTURE_HEIGHT - ((ch / CTR_TEXTURE_CHARS_ROW) * CTR_CHAR_H);

  return (v << 16) | u;
}

template<int SMZX, int HAS_BACKGROUND_TEXTURE, int HAS_TRANSPARENT_COL>
static void ctr_render_layer_inner(struct graphics_data *graphics,
 struct video_layer *vlayer, u32 bufsize)
{
  struct ctr_layer *layer = (struct ctr_layer *)vlayer->platform_layer_data;
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;

  // Flags - dynamically set by the layer generation code.
  boolean draw_background_texture = HAS_BACKGROUND_TEXTURE;
  boolean has_content = false;

  // Cached variables.
  u32 transparent_col = vlayer->transparent_col;
  int offset = vlayer->offset;
  u32 protected_pal_position = graphics->protected_pal_position;

  // Temporary variables for layer generation.
  struct char_element *src = vlayer->data;
  u32 ix, iy;
  int pos0, pos1, pos2, pos3;
  int pos_bg;
  u32 ch, uv;
  u32 col0, col1, col2, col3;

  // SMZX = layer->mode == 0
  // HAS_BACKGROUND_TEXTURE = draw_background_texture
  pos0 = 0;
  pos1 = layer->w * layer->h;
  if(SMZX)
  {
    pos2 = pos1 * 2;
    pos3 = pos1 * 3;
  }

  for(iy = 0; iy < layer->h; iy++)
  {
    for(ix = 0; ix < layer->w; ix++, src++)
    {
      if(HAS_BACKGROUND_TEXTURE)
      {
        pos_bg = morton_lut[(ix & 7) + ((iy & 7) << 3)]
               + ((ix & (~7)) << 3) + ((iy & (~7)) * layer->background.width);
      }

      ch = src->char_value;

      if(ch == INVISIBLE_CHAR)
      {
        if(HAS_BACKGROUND_TEXTURE)
          ((u32 *)layer->background.data)[pos_bg] = 0;
        layer->foreground[pos0++].col = 0;
        layer->foreground[pos1++].col = 0;
        if(SMZX)
        {
          layer->foreground[pos2++].col = 0;
          layer->foreground[pos3++].col = 0;
        }
        continue;
      }
      has_content = true;

      if(ch >= 0x100)
        ch = (ch & 0xFF) + PROTECTED_CHARSET_POSITION;
      else
        ch = (ch + offset) % PROTECTED_CHARSET_POSITION;

      uv = ctr_char_texture_uv(ch);

      if(!SMZX)
      {
        col0 = src->bg_color;
        if(HAS_TRANSPARENT_COL && col0 == transparent_col)
        {
          col0 = 0;
        }
        else
        {
          if(col0 >= 16)
            col0 = (col0 & 0xF) + protected_pal_position;

          col0 = graphics->flat_intensity_palette[col0];
        }

        col1 = src->fg_color;
        if(HAS_TRANSPARENT_COL && col1 == transparent_col)
        {
          col1 = 0;
          draw_background_texture = false;
        }
        else
        {
          if(col1 >= 16)
            col1 = (col1 & 0xF) + protected_pal_position;

          col1 = graphics->flat_intensity_palette[col1];
        }
        if(HAS_BACKGROUND_TEXTURE)
          ((u32 *)layer->background.data)[pos_bg] = col0;
        layer->foreground[pos0].uv = uv;
        layer->foreground[pos0++].col = col0;

        layer->foreground[pos1].uv = uv;
        layer->foreground[pos1++].col = col1;
      }
      else
      {
        u32 idx = ((src->bg_color << 6) | (src->fg_color << 2));
        u32 idx0 = graphics->smzx_indices[idx    ];
        u32 idx1 = graphics->smzx_indices[idx + 1];
        u32 idx2 = graphics->smzx_indices[idx + 2];
        u32 idx3 = graphics->smzx_indices[idx + 3];

        col0 = idx0 == transparent_col ? 0 : graphics->flat_intensity_palette[(u8) idx0];
        col1 = idx1 == transparent_col ? 0 : graphics->flat_intensity_palette[(u8) idx1];
        col2 = idx2 == transparent_col ? 0 : graphics->flat_intensity_palette[(u8) idx2];
        col3 = idx3 == transparent_col ? 0 : graphics->flat_intensity_palette[(u8) idx3];

        if((col1 & col2 & col3) == 0)
          draw_background_texture = false;

        if(HAS_BACKGROUND_TEXTURE)
          ((u32 *)layer->background.data)[pos_bg] = col0;
        layer->foreground[pos0].uv = uv;
        layer->foreground[pos0++].col = col0;
        layer->foreground[pos1].uv = uv;
        layer->foreground[pos1++].col = col1;
        layer->foreground[pos2].uv = uv;
        layer->foreground[pos2++].col = col2;
        layer->foreground[pos3].uv = uv;
        layer->foreground[pos3++].col = col3;
      }
    }
  }

  if(!has_content)
    return;

  if(draw_background_texture)
  {
    C3D_TexFlush(&layer->background);

    ctr_draw_2d_texture(render_data, &layer->background, 0,
     layer->background.height - layer->h, layer->w, layer->h,
     vlayer->x, vlayer->y, layer->w * 8, layer->h * 14,
     (LAYER_DRAWORDER_MAX - layer->draw_order) + 0.66f, 0xffffffff, false);
  }

  ctr_prepare_playfield(render_data, layer->foreground, vlayer->x, vlayer->y, layer->z);
  GSPGPU_FlushDataCache(layer->foreground, sizeof(struct v_char) * bufsize);

  if(!SMZX)
  {
    C3D_TexBind(0, &render_data->charset_vram[0]);
    if(!draw_background_texture)
    {
      C3D_SetTexEnv(0, &(render_data->env_playfield_inv));
      C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, layer->w * layer->h);
    }
    C3D_SetTexEnv(0, &(render_data->env_playfield));
    C3D_DrawArrays(GPU_GEOMETRY_PRIM, layer->w * layer->h, layer->w * layer->h);
  }
  else
  {
    C3D_SetTexEnv(0, &(render_data->env_playfield));
    if(!draw_background_texture)
    {
      C3D_TexBind(0, &render_data->charset_vram[1]);
      C3D_DrawArrays(GPU_GEOMETRY_PRIM, 0, layer->w * layer->h);
    }
    C3D_TexBind(0, &render_data->charset_vram[2]);
    C3D_DrawArrays(GPU_GEOMETRY_PRIM, layer->w * layer->h, layer->w * layer->h);
    C3D_TexBind(0, &render_data->charset_vram[3]);
    C3D_DrawArrays(GPU_GEOMETRY_PRIM, layer->w * layer->h * 2, layer->w * layer->h);
    C3D_TexBind(0, &render_data->charset_vram[4]);
    C3D_DrawArrays(GPU_GEOMETRY_PRIM, layer->w * layer->h * 3, layer->w * layer->h);
  }
}

template<int SMZX, int HAS_BACKGROUND_TEXTURE>
static inline void ctr_render_layer_inner(struct graphics_data *graphics,
 struct video_layer *vlayer, u32 bufsize)
{
  if(vlayer->transparent_col != -1)
    ctr_render_layer_inner<SMZX, HAS_BACKGROUND_TEXTURE, 1>(graphics, vlayer, bufsize);
  else
    ctr_render_layer_inner<SMZX, HAS_BACKGROUND_TEXTURE, 0>(graphics, vlayer, bufsize);
}

template<int SMZX>
static inline void ctr_render_layer_inner(struct graphics_data *graphics,
 struct video_layer *vlayer, u32 bufsize)
{
  struct ctr_layer *layer = (struct ctr_layer *)vlayer->platform_layer_data;
  if(layer->has_background_texture)
    ctr_render_layer_inner<SMZX, 1>(graphics, vlayer, bufsize);
  else
    ctr_render_layer_inner<SMZX, 0>(graphics, vlayer, bufsize);
}

static inline void ctr_render_layer_inner(struct graphics_data *graphics,
 struct video_layer *vlayer, u32 bufsize)
{
  struct ctr_layer *layer = (struct ctr_layer *)vlayer->platform_layer_data;
  if(layer->mode > 0)
    ctr_render_layer_inner<1>(graphics, vlayer, bufsize);
  else
    ctr_render_layer_inner<0>(graphics, vlayer, bufsize);
}

static void ctr_render_layer(struct graphics_data *graphics,
 struct video_layer *vlayer)
{
  struct ctr_layer *layer = (struct ctr_layer *)vlayer->platform_layer_data;
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  u32 bufsize = vlayer->w * vlayer->h * (vlayer->mode > 0 ? 4 : 2);
  u32 max_bufsize = vlayer->w * vlayer->h * 4;
  u32 i;

  if(!ctr_should_render(render_data))
    return;

  if(layer != NULL && (layer->w != vlayer->w || layer->h != vlayer->h))
  {
    linearFree(layer->foreground);
    if(layer->has_background_texture)
      C3D_TexDelete(&(layer->background));
    free(layer);
    layer = NULL;
  }

  if(layer != NULL && (layer->draw_order != vlayer->draw_order))
  {
    layer->draw_order = vlayer->draw_order;
    layer->z = (LAYER_DRAWORDER_MAX - layer->draw_order) + 0.33f;
  }

  if(layer == NULL)
  {
    layer = (struct ctr_layer *)cmalloc(sizeof(struct ctr_layer));
    layer->w = vlayer->w; layer->h = vlayer->h;
    layer->draw_order = vlayer->draw_order;
    layer->z = (LAYER_DRAWORDER_MAX - layer->draw_order) + 0.33f;
    layer->foreground = (struct v_char *)clinearAlloc(sizeof(struct v_char) * max_bufsize, 0x80);

    /**
     * This renderer has two methods to draw char backgrounds. The quicker
     * method is to draw them as single pixels on a texture stretched to the
     * size of the layer. The slower method is to draw the layer an extra time
     * for the background colors.
     *
     * The former method seems to consume a vast amount of buffer space to the
     * point it causes crashes when too many sprites are active. Only use it for
     * the default layers (these are the layers most likely to benefit anyway).
     */
    if((vlayer->draw_order == LAYER_DRAWORDER_BOARD) ||
     (vlayer->draw_order == LAYER_DRAWORDER_OVERLAY) ||
     (vlayer->draw_order == LAYER_DRAWORDER_GAME_UI) ||
     (vlayer->draw_order == LAYER_DRAWORDER_UI))
    {
      layer->has_background_texture = true;
      layer->background.data = NULL;
      C3D_TexInit(&(layer->background), round_to_power_of_two(layer->w),
       round_to_power_of_two(layer->h), GPU_RGBA8);
      C3D_TexSetFilter(&(layer->background), GPU_NEAREST, GPU_NEAREST);
    }
    else
      layer->has_background_texture = false;

    vlayer->platform_layer_data = layer;

    // Initialize the foreground data.
    for(i = 0; i < max_bufsize; i++)
    {
      layer->foreground[i].x = (i % layer->w);
      layer->foreground[i].y = ((i / layer->w) % layer->h);
      layer->foreground[i].uv = 0xFFFFFFFF;
    }
  }

  layer->mode = vlayer->mode;

  ctr_render_layer_inner(graphics, vlayer, bufsize);

  render_data->layer_num++;
}

static void ctr_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  uint32_t flatcolor = graphics->flat_intensity_palette[color];

  if(ctr_should_render(render_data))
  {
    ctr_draw_2d_texture(render_data, NULL, 0, 0, 0, 0,
     x * 8, y * 14 + offset, 8, lines, CTR_LAYER_CURSOR, flatcolor, false);
  }
}

static void ctr_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;
  uint32_t col = 0xFFFFFFFF;

  if(ctr_should_render(render_data))
  {
    C3D_AlphaBlend(GPU_BLEND_SUBTRACT, GPU_BLEND_ADD,
     GPU_SRC_COLOR, GPU_DST_COLOR, GPU_SRC_ALPHA, GPU_DST_ALPHA);
    ctr_draw_2d_texture(render_data, NULL, 0, 0, 0, 0, x, y, w, h, CTR_LAYER_MOUSE, col,
     false);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_SRC_ALPHA,
     GPU_ONE_MINUS_SRC_ALPHA, GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
  }
}

static inline void ctr_draw_playfield(struct ctr_render_data *render_data,
 boolean top_screen)
{
  int x, y;
  int width, height;
  float slider;

  ctr_set_2d_projection_screen(render_data, top_screen);
  if(top_screen)
  {
    if(ctr_keyboard_force_zoom_out())
    {
      slider = 1.0f;
    }
    else
    {
      slider = (osGet3DSliderState() * 1.3f) - 0.15f;

      if(slider < 0.0f) slider = 0.0f;
      if(slider > 1.0f) slider = 1.0f;
    }

    width = 400 + (240 * slider);
    height = 240 + (144 * slider);

    x = render_data->focus_x - (width / 2);
    y = render_data->focus_y - (height / 2);

    if(x < 0)
      x = 0;

    if((x + width) > 640)
      x = 640 - width;

    if(height > 350)
    {
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex,
       x, 512 - 350, width, 350, 0, (240 - (240 * 350 / height)) / 2,
       400, 240 * 350 / height, 2.0f, 0xffffffff, true);
    }
    else
    {
      if(y < 0)
        y = 0;

      if((y + height) > 350)
        y = 350 - height;

      ctr_draw_2d_texture(render_data, &render_data->playfield_tex,
       x, 512 - y - height, width, height, 0, 0, 400, 240,
       2.0f, 0xffffffff, true);
    }
  }
  else
  {
    if(get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
    {
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex,
       0, 512 - 350, 640, 350, 80, 12.75, 160, 87.5, 2.0f, 0xffffffff, true);
    }
    else
    {
      int width = 320, height = ctr_get_subscreen_height();
      ctr_draw_2d_texture(render_data, &render_data->playfield_tex,
        0, 512 - 350, 640, 350,
        (int) ((320 - width) / 2), (int) ((240 - height) / 2),
        width, height, 2.0f, 0xffffffff, true
      );
    }
  }
}

static void ctr_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;

  if(!ctr_should_render(render_data))
  {
    render_data->checked_frame = false;
    return;
  }

#ifndef RDR_DEBUG
  C3D_RenderTargetClear(render_data->target_top, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_FrameDrawOn(render_data->target_top);
  ctr_draw_playfield(render_data, true);
#endif

  C3D_RenderTargetClear(render_data->target_bottom, C3D_CLEAR_ALL, 0x000000, 0);
  C3D_FrameDrawOn(render_data->target_bottom);
  if(get_bottom_screen_mode() == BOTTOM_SCREEN_MODE_KEYBOARD)
  {
    ctr_set_2d_projection_screen(render_data, false);
    ctr_keyboard_draw(render_data);
  }

  ctr_draw_playfield(render_data, false);

  render_data->cursor_on = 0;
  render_data->mouse_on = 0;

  C3D_FrameEnd(0);
  render_data->rendering_frame = false;
  render_data->checked_frame = false;
}

static void ctr_focus_pixel(struct graphics_data *graphics,
 unsigned int x, unsigned int y)
{
  struct ctr_render_data *render_data = (struct ctr_render_data *)graphics->render_data;

  switch(get_allow_focus_changes())
  {
    case FOCUS_FORBID:
      return;
    case FOCUS_ALLOW:
      if(render_data->last_focus_x != x || render_data->last_focus_y != y)
      {
        render_data->last_focus_x = x;
        render_data->last_focus_y = y;
        render_data->focus_x = x;
        render_data->focus_y = y;
      }
      break;
    case FOCUS_PASS:
      render_data->focus_x = x;
      render_data->focus_y = y;
      break;
  }
}

const struct renderer renderer_ctr =
{
  "3ds",
  NULL,
  ctr_init_video,
  ctr_free_video,
  ctr_create_window,
  NULL,
  NULL,
  set_window_viewport_centered,
  NULL,
  NULL,
  NULL,
  ctr_update_colors,
  ctr_remap_char_range,
  ctr_remap_char,
  ctr_remap_charbyte,
  NULL,
  NULL,
  ctr_render_layer,
  ctr_render_cursor,
  NULL,
  ctr_render_mouse,
  ctr_sync_screen,
  ctr_focus_pixel,
};
