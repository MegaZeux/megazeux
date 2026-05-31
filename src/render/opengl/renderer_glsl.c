/* MegaZeux
 *
 * Copyright (C) 2008 Joel Bouchard Lamontagne <logicow@gmail.com>
 * Copyright (C) 2006-2007 Gilead Kutnick <exophase@adelphia.net>
 * Copyright (C) 2009 Alistair John Strachan <alistair@devzero.co.uk>
 * Copyright (C) 2017 Dr Lancer-X <drlancer@megazeux.org>
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

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "../render.h"
#include "../render_layer.h"
#include "../renderers.h"

#include "../../util.h"
#include "../../io/path.h"
#include "../../io/vio.h"
#include "../../platform/platform_dso.h"

// Uncomment to enable GL debug output (requires OpenGL 4.3+).
//#define ENABLE_GL_DEBUG_OUTPUT 1

#ifdef CONFIG_SDL
#include "../sdl/render_sdl.h"
#ifdef ENABLE_GL_DEBUG_OUTPUT
#if SDL_VERSION_ATLEAST(3,0,0)
#include <SDL3/SDL_opengl_glext.h>
#else
#include <SDL_opengl_glext.h>
#endif
#endif
#endif

#ifdef CONFIG_EGL
#include <GLES2/gl2.h>
#include "render_egl.h"
#endif

#ifdef CONFIG_GLES
typedef GLenum GLiftype;
#else
typedef GLint GLiftype;
#endif

#include "render_gl.h"

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER GL_FRAMEBUFFER_EXT
#endif

#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0 GL_COLOR_ATTACHMENT0_EXT
#endif

static const struct gl_version gl_required_version = { 2, 0 };

#define CHARSET_COLS 64
#define CHARSET_ROWS (FULL_CHARSET_SIZE / CHARSET_COLS)
#define BG_WIDTH 128
#define BG_HEIGHT 32

#define SCREEN_PIX_W_F (1.0f * SCREEN_PIX_W)
#define SCREEN_PIX_H_F (1.0f * SCREEN_PIX_H)

// Must be powers of 2
#define TEX_DATA_WIDTH 512
#define TEX_DATA_HEIGHT 1024

// Charsets: CHARSET_COLS * CHAR_W (512) x CHARSET_ROWS * CHAR_H (896)
#define TEX_DATA_CHR_X 0
#define TEX_DATA_CHR_Y 0

// Palette: FULL_PAL_SIZE + 1 (273) x 1
#define TEX_DATA_PAL_X 0
#define TEX_DATA_PAL_Y 896

// Indicies: SMZX_PAL_SIZE (256) x 4
#define TEX_DATA_IDX_X 0
#define TEX_DATA_IDX_Y 897

// Layer: 81 x 26 (max)
#define TEX_DATA_LAYER_X 0
#define TEX_DATA_LAYER_Y 901

// NOTE: Layer data packing scheme
// This is intentionally wasteful so the components don't interfere with each
// other on old and embedded cards with poor float precision.
// C = char.
// B = background color (0-15 normal; >=16 protected)
// F = foreground color (0-15 normal; >=16 protected)
// For SMZX, the subpalette number P is sent in place of B and F. If this value
// is FULL_PAL_SIZE, the char will be treated as transparent. If B or F is sent
// as PAL_SIZE + PROTECTED_PAL_SIZE, that individual color will be transparent.
// w        z        y        x
// 00000000 00000000 00000000 00000000
// CCCCCCCC CCCCCCCC BBBBBBBB FFFFFFFF
#define LAYER_SUBPALETTE_POS 0
#define LAYER_FG_POS 0
#define LAYER_BG_POS 8
#define LAYER_CHAR_POS 16

enum
{
  TEX_SCREEN_ID,
  TEX_DATA_ID,
  NUM_TEXTURES
};

enum
{
  FBO_SCREEN_TEX,
  NUM_FBOS
};

enum
{
  ATTRIB_POSITION,
  ATTRIB_TEXCOORD,
  ATTRIB_COLOR,
};

/**
 * Some GL drivers attempt to run GLSL in software, resulting in extremely poor
 * performance for MegaZeux. When one of the drivers in this blacklist is
 * detected, video output will fall back to a different renderer instead.
 */
struct blacklist_entry
{
  const char *match_string; // Compare with GL_RENDERER output to detect driver.
  const char *reason;       // Reason for blacklisting. Displays to the user.
};

static struct blacklist_entry auto_glsl_blacklist[] =
{
  { "swrast",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "softpipe",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "llvmpipe",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "Software Rasterizer",
    "  MESA software renderer.\n"
    "  Blacklisted due to poor performance on some machines.\n" },
  { "Chromium",
    "  Chromium redirects GL commands to other GL drivers. The destination\n"
    "  often seems to be a software renderer, causing poor performance on\n"
    "  some machines.\n" },
  { "Intel EMGD",
    "  This driver may have extremely questionable \"OpenGL 2.0\" support.\n"
    "  It may output a wall of spurious/nonsensical warnings when compiling\n"
    "  the shaders, and it may claim that these shaders compiled\n"
    "  successfully in cases where they actually did not.\n" },
};

static int auto_glsl_blacklist_len = ARRAY_SIZE(auto_glsl_blacklist);

static struct
{
  void (GL_APIENTRY *glAttachShader)(GLuint program, GLuint shader);
  void (GL_APIENTRY *glBindAttribLocation)(GLuint program, GLuint index,
   const char *name);
  void (GL_APIENTRY *glBindTexture)(GLenum target, GLuint texture);
  void (GL_APIENTRY *glBlendFunc)(GLenum sfactor, GLenum dfactor);
  void (GL_APIENTRY *glClear)(GLbitfield mask);
  void (GL_APIENTRY *glClearColor)(GLclampf red, GLclampf green,
   GLclampf blue, GLclampf alpha);
  void (GL_APIENTRY *glCompileShader)(GLuint shader);
  void (GL_APIENTRY *glCopyTexImage2D)(GLenum target, GLint level,
   GLenum internalformat, GLint x, GLint y, GLsizei width, GLsizei height,
   GLint border);
  GLenum (GL_APIENTRY *glCreateProgram)(void);
  GLenum (GL_APIENTRY *glCreateShader)(GLenum type);
  void (GL_APIENTRY *glDisable)(GLenum cap);
  void (GL_APIENTRY *glDisableVertexAttribArray)(GLuint index);
  void (GL_APIENTRY *glDrawArrays)(GLenum mode, GLint first, GLsizei count);
  void (GL_APIENTRY *glEnable)(GLenum cap);
  void (GL_APIENTRY *glEnableVertexAttribArray)(GLuint index);
  void (GL_APIENTRY *glGenTextures)(GLsizei n, GLuint *textures);
  void (GL_APIENTRY *glDeleteTextures)(GLsizei n, GLuint *textures);
  GLenum (GL_APIENTRY *glGetError)(void);
  void (GL_APIENTRY *glGetProgramInfoLog)(GLuint program, GLsizei bufsize,
   GLsizei *len, char *infolog);
  void (GL_APIENTRY *glGetShaderiv)(GLuint shader, GLenum pname, GLint *params);
  void (GL_APIENTRY *glGetShaderInfoLog)(GLuint shader, GLsizei bufsize,
   GLsizei *len, char *infolog);
  const GLubyte* (GL_APIENTRY *glGetString)(GLenum name);
  void (GL_APIENTRY *glLinkProgram)(GLuint program);
  void (GL_APIENTRY *glShaderSource)(GLuint shader, GLsizei count,
   const char **strings, const GLint *length);
  void (GL_APIENTRY *glTexImage2D)(GLenum target, GLint level,
   GLiftype internalformat, GLsizei width, GLsizei height, GLint border,
   GLenum format, GLenum type, const GLvoid *pixels);
  void (GL_APIENTRY *glTexParameterf)(GLenum target, GLenum pname,
   GLfloat param);
  void (GL_APIENTRY *glTexSubImage2D)(GLenum target, GLint level,
   GLint xoffset, GLint yoffset, GLsizei width, GLsizei height,
   GLenum format, GLenum type, const void *pixels);
  void (GL_APIENTRY *glUseProgram)(GLuint program);
  void (GL_APIENTRY *glVertexAttribPointer)(GLuint index, GLint size,
   GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);
  void (GL_APIENTRY *glViewport)(GLint x, GLint y, GLsizei width,
   GLsizei height);
  void (GL_APIENTRY *glDeleteShader)(GLuint shader);
  void (GL_APIENTRY *glDetachShader)(GLuint program, GLuint shader);
  void (GL_APIENTRY *glDeleteProgram)(GLuint program);
  void (GL_APIENTRY *glGetAttachedShaders)(GLuint program, GLsizei maxCount,
   GLsizei *count, GLuint *shaders);
  GLint (GL_APIENTRY *glGetUniformLocation)(GLuint program, const char *name);
  void (GL_APIENTRY *glUniform1f)(GLint location, GLfloat v0);
#ifdef CONFIG_GLES
  void (GL_APIENTRY *glGetShaderPrecisionFormat)(GLenum shaderType,
   GLenum precisionType, GLint *range, GLint *precision);
#endif
#ifdef ENABLE_GL_DEBUG_OUTPUT
  void (GL_APIENTRY *glDebugMessageCallback)(GLDEBUGPROC callback, void *param);
#endif

  // FBO functions are optional for GL (requires 3.0+), mandatory for GLES.
  boolean has_fbo;
  void (GL_APIENTRY *glGenFramebuffers)(GLsizei n, GLuint *ids);
  void (GL_APIENTRY *glDeleteFramebuffers)(GLsizei n, GLuint *framebuffers);
  void (GL_APIENTRY *glBindFramebuffer)(GLenum target, GLuint framebuffer);
  void (GL_APIENTRY *glFramebufferTexture2D)(GLenum target, GLenum attachment,
   GLenum textarget, GLuint texture, GLint level);
}
glsl;

static const struct dso_syms_map glsl_syms_map[] =
{
  { "glAttachShader",             { &glsl.glAttachShader }},
  { "glBindAttribLocation",       { &glsl.glBindAttribLocation }},
  { "glBindTexture",              { &glsl.glBindTexture }},
  { "glBlendFunc",                { &glsl.glBlendFunc }},
  { "glClear",                    { &glsl.glClear }},
  { "glClearColor",               { &glsl.glClearColor }},
  { "glCompileShader",            { &glsl.glCompileShader }},
  { "glCopyTexImage2D",           { &glsl.glCopyTexImage2D }},
  { "glCreateProgram",            { &glsl.glCreateProgram }},
  { "glCreateShader",             { &glsl.glCreateShader }},
  { "glDeleteProgram",            { &glsl.glDeleteProgram }},
  { "glDeleteShader",             { &glsl.glDeleteShader }},
  { "glDeleteTextures",           { &glsl.glDeleteTextures }},
  { "glDetachShader",             { &glsl.glDetachShader }},
  { "glDisable",                  { &glsl.glDisable }},
  { "glDisableVertexAttribArray", { &glsl.glDisableVertexAttribArray }},
  { "glDrawArrays",               { &glsl.glDrawArrays }},
  { "glEnable",                   { &glsl.glEnable }},
  { "glEnableVertexAttribArray",  { &glsl.glEnableVertexAttribArray }},
  { "glGenTextures",              { &glsl.glGenTextures }},
  { "glGetAttachedShaders",       { &glsl.glGetAttachedShaders }},
  { "glGetError",                 { &glsl.glGetError }},
  { "glGetProgramInfoLog",        { &glsl.glGetProgramInfoLog }},
  { "glGetShaderInfoLog",         { &glsl.glGetShaderInfoLog }},
  { "glGetShaderiv",              { &glsl.glGetShaderiv }},
  { "glGetString",                { &glsl.glGetString }},
  { "glGetUniformLocation",       { &glsl.glGetUniformLocation }},
  { "glLinkProgram",              { &glsl.glLinkProgram }},
  { "glShaderSource",             { &glsl.glShaderSource }},
  { "glTexImage2D",               { &glsl.glTexImage2D }},
  { "glTexParameterf",            { &glsl.glTexParameterf }},
  { "glTexSubImage2D",            { &glsl.glTexSubImage2D }},
  { "glUniform1f",                { &glsl.glUniform1f }},
  { "glUseProgram",               { &glsl.glUseProgram }},
  { "glVertexAttribPointer",      { &glsl.glVertexAttribPointer }},
  { "glViewport",                 { &glsl.glViewport }},
#ifdef CONFIG_GLES
  { "glGetShaderPrecisionFormat", { &glsl.glGetShaderPrecisionFormat }},
#endif
#ifdef ENABLE_GL_DEBUG_OUTPUT
  { "glDebugMessageCallback",     { &glsl.glDebugMessageCallback }},
#endif
  DSO_MAP_END
};

static const struct dso_syms_map glsl_syms_map_fbo[] =
{
  { "glBindFramebuffer",          { &glsl.glBindFramebuffer }},
  { "glDeleteFramebuffers",       { &glsl.glDeleteFramebuffers }},
  { "glFramebufferTexture2D",     { &glsl.glFramebufferTexture2D }},
  { "glGenFramebuffers",          { &glsl.glGenFramebuffers }},
  DSO_MAP_END
};

#define gl_check_error() gl_error(__FILE__, __LINE__, glsl.glGetError)

struct glsl_render_data
{
#ifdef CONFIG_EGL
  struct egl_render_data egl;
#endif
#ifdef CONFIG_SDL
  struct sdl_render_data sdl;
#endif
  uint32_t *pixels;
  uint32_t charset_texture[CHAR_H * FULL_CHARSET_SIZE * CHAR_W];
  uint32_t background_texture[BG_WIDTH * BG_HEIGHT];
  GLuint textures[NUM_TEXTURES];
  GLuint fbos[NUM_FBOS];
  GLuint uniform_tilemap_pro_pal;
  GLubyte palette[3 * FULL_PAL_SIZE];
  boolean remap_texture;
  boolean remap_char[FULL_CHARSET_SIZE];
  boolean dirty_palette;
  boolean dirty_indices;
  boolean use_software_renderer;
  int last_tcol;
  GLuint scaler_program;
  GLuint tilemap_program;
  GLuint tilemap_smzx_program;
  GLuint mouse_program;
  GLuint cursor_program;
  struct config_info *conf;
};

static char *source_cache[GLSL_SHADER_RES_COUNT];

#define INFO_MAX 1000

static GLint glsl_verify_compile(struct glsl_render_data *render_data,
 GLenum shader)
{
  GLint compile_status;
  char buffer[INFO_MAX];
  GLsizei len = 0;

  glsl.glGetShaderiv(shader, GL_COMPILE_STATUS, &compile_status);
  glsl.glGetShaderInfoLog(shader, INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);

  if(compile_status == GL_FALSE)
    warn("Shader compilation failed!\n");

  return compile_status;
}

static void glsl_verify_link(struct glsl_render_data *render_data,
 GLenum program)
{
  char buffer[INFO_MAX];
  GLsizei len = 0;

  glsl.glGetProgramInfoLog(program, INFO_MAX - 1, &len, buffer);
  buffer[len] = 0;

  if(len > 0)
    warn("%s", buffer);
}

static char *glsl_load_string(const char *filename)
{
  char *buffer = NULL;
  unsigned long size;
  vfile *vf;

  vf = vfopen_unsafe(filename, "rb");
  if(!vf)
    goto err_out;

  size = vfilelength(vf, false);
  if(size <= 0)
    goto err_close;

  buffer = cmalloc(size + 1);
  buffer[size] = '\0';

  if(vfread(buffer, size, 1, vf) != 1)
  {
    free(buffer);
    buffer = NULL;
  }

err_close:
  vfclose(vf);
err_out:
  return buffer;
}

static boolean glsl_scaling_shader_is_default(struct graphics_data *graphics)
{
  return graphics->gl_scaling_shader[0] == '\0';
}

/**
 * Sets the name of the current scaling shader. If shader is NULL, this will be
 * set to the default scaling shader.
 */
static void glsl_set_scaling_shader(struct graphics_data *graphics,
 const char *shader)
{
  if(shader)
  {
    size_t buf_len = ARRAY_SIZE(graphics->gl_scaling_shader);
    snprintf(graphics->gl_scaling_shader, buf_len, "%s", shader);
    graphics->gl_scaling_shader[buf_len - 1] = '\0';
  }
  else
    graphics->gl_scaling_shader[0] = '\0';
}

static GLuint glsl_load_shader(struct graphics_data *graphics,
 enum resource_id res, GLenum type)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int index = res - GLSL_SHADER_RES_FIRST;
  int loaded_config = 0;
  boolean is_user_scaler = false;
  GLint compile_status;
  GLenum shader;

  assert(res >= GLSL_SHADER_RES_FIRST && res <= GLSL_SHADER_RES_LAST);

  if(res == GLSL_SHADER_SCALER_VERT || res == GLSL_SHADER_SCALER_FRAG)
    if(!glsl_scaling_shader_is_default(graphics))
      is_user_scaler = true;

  // If we've already seen this shader, it's loaded, and we don't
  // need to do any more file I/O.

  if(!source_cache[index])
  {
    if(is_user_scaler)
    {
      // Try to load these from config before loading the default.

      char *path = cmalloc(MAX_PATH);
      size_t path_len = path_clean_copy(path, MAX_PATH,
       mzx_res_get_by_id(GLSL_SHADER_SCALER_DIRECTORY));

      switch(res)
      {
        case GLSL_SHADER_SCALER_VERT:
          snprintf(path + path_len, MAX_PATH - path_len,
           DIR_SEPARATOR "%s.vert", graphics->gl_scaling_shader);
          break;

        case GLSL_SHADER_SCALER_FRAG:
          snprintf(path + path_len, MAX_PATH - path_len,
           DIR_SEPARATOR "%s.frag", graphics->gl_scaling_shader);
          break;

        default:
          break;
      }
      path[MAX_PATH - 1] = '\0';

      source_cache[index] = glsl_load_string(path);

      if(source_cache[index])
      {
        loaded_config = 1;
      }
      else
      {
        debug("Failed to find shader '%s'; falling back to default\n", path);
        loaded_config = 0;
      }

      free(path);
    }

    if(!loaded_config)
    {
      is_user_scaler = false;

      if(res == GLSL_SHADER_SCALER_FRAG)
        glsl_set_scaling_shader(graphics, NULL);

      source_cache[index] = glsl_load_string(mzx_res_get_by_id(res));
      if(!source_cache[index])
      {
        warn("Failed to find required shader '%s'\n", mzx_res_get_by_id(res));
        return 0;
      }
    }
  }

  shader = glsl.glCreateShader(type);

#ifdef CONFIG_GLES
  {
    /**
     * OpenGL ES really doesn't like '#version 110' being specified. This
     * is unfortunate, because some drivers (e.g. Intel HD) will emit warnings
     * if it isn't explicit. Comment these directives out if found.
     */

    char *pos = source_cache[index];

    while((pos = strstr(pos, "#version 110")))
    {
      debug("Found '#version 110' at %d in %s; commenting out\n",
       (int)(pos - source_cache[index]),
       mzx_res_get_by_id(res));
      pos[0] = '/';
      pos[1] = '/';
    }
  }
#endif // CONFIG_GLES

#ifdef CONFIG_GLES
  {
    const GLchar *sources[2];
    GLint lengths[2];

    sources[0] = "precision mediump float;";
    sources[1] = source_cache[index];

    lengths[0] = (GLint)strlen(sources[0]);
    lengths[1] = (GLint)strlen(source_cache[index]);

    glsl.glShaderSource(shader, 2, sources, lengths);
  }
#else
  {
    GLint length = (GLint)strlen(source_cache[index]);
    const char **source_ptr = (const char **)&source_cache[index];

    glsl.glShaderSource(shader, 1, source_ptr, &length);
  }
#endif

  glsl.glCompileShader(shader);
  compile_status = glsl_verify_compile(render_data, shader);

  if(compile_status == GL_FALSE)
  {
    if(is_user_scaler)
    {
      // Attempt the default shader
      glsl_set_scaling_shader(graphics, NULL);
      free(source_cache[index]);
      source_cache[index] = NULL;

      return glsl_load_shader(graphics, res, type);
    }
    else
    {
      warn("Required shader '%s' failed to compile.\n", mzx_res_get_by_id(res));
      return 0;
    }
  }

  return shader;
}

static GLuint glsl_load_program(struct graphics_data *graphics,
 enum resource_id v_res, enum resource_id f_res)
{
  GLenum vertex, fragment;
  GLuint program = 0;
  char *path;

  path = cmalloc(MAX_PATH);

  vertex = glsl_load_shader(graphics, v_res, GL_VERTEX_SHADER);
  if(!vertex)
    goto err_free_path;

  fragment = glsl_load_shader(graphics, f_res, GL_FRAGMENT_SHADER);
  if(!fragment)
    goto err_free_path;

  program = glsl.glCreateProgram();

  glsl.glAttachShader(program, vertex);
  glsl.glAttachShader(program, fragment);

err_free_path:
  free(path);
  return program;
}

static void glsl_delete_shaders(GLuint program)
{
  GLuint shaders[16];
  GLsizei shader_count, i;
  glsl.glGetAttachedShaders(program, 16, &shader_count, shaders);
  gl_check_error();
  for(i = 0; i < shader_count; i++)
  {
    glsl.glDetachShader(program, shaders[i]);
    gl_check_error();
    glsl.glDeleteShader(shaders[i]);
    gl_check_error();
  }
}

static void glsl_load_shaders(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;

  render_data->scaler_program = glsl_load_program(graphics,
   GLSL_SHADER_SCALER_VERT, GLSL_SHADER_SCALER_FRAG);
  if(render_data->scaler_program)
  {
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->scaler_program);
    glsl_verify_link(render_data, render_data->scaler_program);
    glsl_delete_shaders(render_data->scaler_program);
  }

  if(render_data->use_software_renderer)
    return;

  render_data->tilemap_program = glsl_load_program(graphics,
   GLSL_SHADER_TILEMAP_VERT, GLSL_SHADER_TILEMAP_FRAG);
  if(render_data->tilemap_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_program);
    glsl_verify_link(render_data, render_data->tilemap_program);
    glsl_delete_shaders(render_data->tilemap_program);

    render_data->uniform_tilemap_pro_pal = glsl.glGetUniformLocation(
     render_data->tilemap_program, "protected_pal_position");
  }

  render_data->tilemap_smzx_program = glsl_load_program(graphics,
   GLSL_SHADER_TILEMAP_VERT, GLSL_SHADER_TILEMAP_SMZX_FRAG);
  if(render_data->tilemap_smzx_program)
  {
    glsl.glBindAttribLocation(render_data->tilemap_smzx_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->tilemap_smzx_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->tilemap_smzx_program);
    glsl_verify_link(render_data, render_data->tilemap_smzx_program);
    glsl_delete_shaders(render_data->tilemap_smzx_program);
  }

  render_data->mouse_program = glsl_load_program(graphics,
   GLSL_SHADER_MOUSE_VERT, GLSL_SHADER_MOUSE_FRAG);
  if(render_data->mouse_program)
  {
    glsl.glBindAttribLocation(render_data->mouse_program,
     ATTRIB_POSITION, "Position");
    glsl.glLinkProgram(render_data->mouse_program);
    glsl_verify_link(render_data, render_data->mouse_program);
    glsl_delete_shaders(render_data->mouse_program);
  }

  render_data->cursor_program  = glsl_load_program(graphics,
   GLSL_SHADER_CURSOR_VERT, GLSL_SHADER_CURSOR_FRAG);
  if(render_data->cursor_program)
  {
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->cursor_program,
     ATTRIB_COLOR, "Color");
    glsl.glLinkProgram(render_data->cursor_program);
    glsl_verify_link(render_data, render_data->cursor_program);
    glsl_delete_shaders(render_data->cursor_program);
  }
}

static boolean glsl_init_video(struct graphics_data *graphics,
 struct config_info *conf)
{
  struct glsl_render_data *render_data =
   (struct glsl_render_data *)cmalloc(sizeof(struct glsl_render_data));
  if(!render_data)
    return false;

  if(!GL_LoadLibrary(GL_LIB_PROGRAMMABLE))
    goto err_free;

  memset(render_data, 0, sizeof(struct glsl_render_data));
  graphics->render_data = render_data;

  graphics->gl_vsync = conf->gl_vsync;
  graphics->allow_resize = conf->allow_resize;
  graphics->gl_filter_method = conf->gl_filter_method;
  graphics->ratio = conf->video_ratio;
  graphics->bits_per_pixel = 32;

  glsl_set_scaling_shader(graphics, conf->gl_scaling_shader);

  // OpenGL only supports 16/32bit colour
  if(conf->force_bpp == 16 || conf->force_bpp == 32)
    graphics->bits_per_pixel = conf->force_bpp;

  // Buffer for screen texture
  render_data->pixels = (uint32_t *)cmalloc(sizeof(uint32_t) *
   GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);
  render_data->conf = conf;
  if(!render_data->pixels)
    goto err_free;

  return true;

err_free:
  free(render_data);
  graphics->render_data = NULL;
  return false;
}

static void glsl_free_video(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int i;

  for(i = 0; i < GLSL_SHADER_RES_COUNT; i++)
  {
    if(source_cache[i])
    {
      free((void *)source_cache[i]);
      source_cache[i] = NULL;
    }
  }

  if(render_data)
  {
    if(glsl.has_fbo)
    {
      glsl.glDeleteFramebuffers(NUM_FBOS, render_data->fbos);
      gl_check_error();
    }

    if(glsl.glDeleteTextures)
    {
      glsl.glDeleteTextures(NUM_TEXTURES, render_data->textures);
      gl_check_error();

      glsl.glUseProgram(0);
      gl_check_error();
    }

    gl_cleanup(graphics);
    free(render_data->pixels);
    free(render_data);
    graphics->render_data = NULL;
  }
}

static void glsl_remap_char_range(struct graphics_data *graphics, uint16_t first,
 uint16_t count)
{
  struct glsl_render_data *render_data = graphics->render_data;

  if(first + count > FULL_CHARSET_SIZE)
    count = FULL_CHARSET_SIZE - first;

  // FIXME arbitrary
  if(count <= 256)
    memset(render_data->remap_char + first, true, count);

  else
    render_data->remap_texture = true;
}

static boolean glsl_resize_callback(struct graphics_data *graphics,
 struct video_window *window)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int width = window->width_px;
  int height = window->height_px;

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  // Initial clear color <0,0,0,0> may be interpreted as transparent (Wayland).
  glsl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl_check_error();
  // Clear entire window to prevent background leak-through.
  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();

  // Free any preexisting textures if they exist
  glsl.glDeleteTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  glsl.glGenTextures(NUM_TEXTURES, render_data->textures);
  gl_check_error();

  // Screen texture
  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
  gl_check_error();

  gl_set_filter_method(CONFIG_GL_FILTER_LINEAR, glsl.glTexParameterf);

  glsl.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  // Fill the entire screen texture transparent to avoid edge leak-through.
  memset(render_data->pixels, 0,
   sizeof(uint32_t) * GL_POWER_2_WIDTH * GL_POWER_2_HEIGHT);

  glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GL_POWER_2_WIDTH,
   GL_POWER_2_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE,
   render_data->pixels);
  gl_check_error();

  if(glsl.has_fbo)
  {
    glsl.glDeleteFramebuffers(NUM_FBOS, render_data->fbos);
    gl_check_error();

    glsl.glGenFramebuffers(NUM_FBOS, render_data->fbos);
    gl_check_error();

    glsl.glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbos[FBO_SCREEN_TEX]);
    gl_check_error();

    glsl.glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
     GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID], 0);
  }

  // Data texture
  if(!render_data->use_software_renderer)
  {
    glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
    gl_check_error();

    gl_set_filter_method(CONFIG_GL_FILTER_NEAREST, glsl.glTexParameterf);

    glsl.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, TEX_DATA_WIDTH, TEX_DATA_HEIGHT,
     0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    gl_check_error();

    glsl_remap_char_range(graphics, 0, FULL_CHARSET_SIZE);
    render_data->dirty_palette = true;
    render_data->dirty_indices = true;
  }

  glsl_load_shaders(graphics);
  return true;
}

#ifdef ENABLE_GL_DEBUG_OUTPUT
static void glsl_debug_callback(GLenum source, GLenum type, GLuint id,
 GLenum severity, GLsizei length, const GLchar *message, const void *userParam)
{
  length = length < 0 ? (GLsizei)strlen(message) : length;
  debug("GL (source 0x%x, type 0x%x, severity 0x%x): %.*s\n",
    source, type, severity, length, message
  );
}
#endif

static boolean glsl_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
#ifdef CONFIG_GLES
  struct glsl_render_data *render_data = graphics->render_data;
#endif
  boolean load_fbo_syms = true;

  gl_set_attributes(graphics);

  if(!gl_create_window(graphics, window, gl_required_version))
    return false;

  gl_set_attributes(graphics);

  if(!gl_load_syms(glsl_syms_map))
  {
    gl_cleanup(graphics);
    return false;
  }

  // GLSL needs a specific version of OpenGL; desktop GL must be 2.0.
  // All OpenGL ES 2.0 implementations are supported, so don't do
  // the check with these configurations.
#ifndef CONFIG_GLES
  {
    const char *version;
    double version_float;

    version = (const char *)glsl.glGetString(GL_VERSION);
    if(!version)
    {
      warn("Could not load GL version string.\n");
      gl_cleanup(graphics);
      return false;
    }

    version_float = atof(version);
    if(version_float < 2.0)
    {
      warn("Need >= OpenGL 2.0, got OpenGL %.1f.\n", version_float);
      gl_cleanup(graphics);
      return false;
    }

    load_fbo_syms = false;
    if(version_float >= 3.0)
    {
      debug("Attempting to load FBO syms...\n");
      load_fbo_syms = true;
    }
  }
#else
  if(!render_data->use_software_renderer)
  {
    // All OpenGL ES 2.0 implementations are "technically" supported, but some
    // may not support highp floats and may not provide adequate precision in
    // their mediump floats. Print some warnings in this case.
    GLint range[2];
    GLint precision;

    glsl.glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT,
     range, &precision);
    if(precision == 0)
    {
      warn("no support for high-precision floats in the fragment shader!\n");

      glsl.glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_MEDIUM_FLOAT,
       range, &precision);
      warn("fragment mediump float range = (2^-%d, 2^%d), precision = 2^-%d\n",
       range[0], range[1], precision);
      if(range[0] <= 11 || precision <= 10)
      {
        warn("poor medium float precision! "
         "This renderer may look bad; use \"glslscale\" or \"softscale\" instead.\n");
      }
    }
  }
#endif

  if(load_fbo_syms && gl_load_syms(glsl_syms_map_fbo))
  {
    debug("Using FBO syms for GLSL renderer.\n");
    glsl.has_fbo = true;
  }
  else
    glsl.has_fbo = false;

#ifdef ENABLE_GL_DEBUG_OUTPUT
  glsl.glEnable(GL_DEBUG_OUTPUT);
  glsl.glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glsl.glDebugMessageCallback(glsl_debug_callback, NULL);
#endif

  return glsl_resize_callback(graphics, window);
}

static boolean glsl_software_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->use_software_renderer = true;
  return glsl_create_window(graphics, window);
}

static boolean glsl_auto_create_window(struct graphics_data *graphics,
 struct video_window *window)
{
  struct glsl_render_data *render_data = graphics->render_data;
  const char *renderer;
  int i;

  if(glsl_software_create_window(graphics, window))
  {
    // Driver blacklist. If the renderer is on the blacklist, fall back
    // on software as these renderers have disastrous GLSL performance.

    renderer = (const char *)glsl.glGetString(GL_RENDERER);

    // Print the full renderer string for reference.
    info("GL driver: %s\n", renderer);
    info("GL version: %s\n\n", (const char *)glsl.glGetString(GL_VERSION));

    for(i = 0; i < auto_glsl_blacklist_len; i++)
    {
      if(strstr(renderer, auto_glsl_blacklist[i].match_string))
      {
        warn(
          "Detected blacklisted driver: \"%s\". Disabling glsl. Reason:\n\n"
          "%s\n\n"
          "Run again with \"video_output=glsl\" or \"video_output=glslscale\" "
          "to force-enable glsl.\n\n",
          auto_glsl_blacklist[i].match_string,
          auto_glsl_blacklist[i].reason
        );
        gl_cleanup(graphics);
        return false;
      }
    }

    // Switch from auto_glsl to glslscale now that it is known to work.
    graphics->renderer = &renderer_glslscale;
    strcpy(render_data->conf->video_output, "glslscale");

    return true;
  }
  return false;
}

static void glsl_remap_char(struct graphics_data *graphics, uint16_t chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  render_data->remap_char[chr] = true;
}

static void glsl_remap_charbyte(struct graphics_data *graphics,
 uint16_t chr, uint8_t byte)
{
  glsl_remap_char(graphics, chr);
}

static int *glsl_char_bitmask_to_texture(signed char *c, int *p)
{
  // This tests the 7th bit, if 0, result is 0.
  // If 1, result is 255 (because of sign extended bit shift!).
  // Note the use of char constants to force 8bit calculation.
  *(p++) = *c << 24 >> 31;
  *(p++) = *c << 25 >> 31;
  *(p++) = *c << 26 >> 31;
  *(p++) = *c << 27 >> 31;
  *(p++) = *c << 28 >> 31;
  *(p++) = *c << 29 >> 31;
  *(p++) = *c << 30 >> 31;
  *(p++) = *c << 31 >> 31;
  return p;
}

static inline void glsl_do_remap_charsets(struct graphics_data *graphics)
{
  struct glsl_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i, j, k;

  for(i = 0; i < CHARSET_ROWS; i++, c += -CHAR_H + CHARSET_COLS * CHAR_H)
    for(j = 0; j < CHAR_H; j++, c += -CHARSET_COLS * CHAR_H + 1)
      for(k = 0; k < CHARSET_COLS; k++, c += CHAR_H)
        p = glsl_char_bitmask_to_texture(c, p);

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_CHR_X,
   TEX_DATA_CHR_Y,
   CHARSET_COLS * CHAR_W,
   CHARSET_ROWS * CHAR_H,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static inline void glsl_do_remap_char(struct graphics_data *graphics,
 uint16_t chr)
{
  struct glsl_render_data *render_data = graphics->render_data;
  signed char *c = (signed char *)graphics->charset;
  int *p = (int *)render_data->charset_texture;
  unsigned int i;

  c += chr * 14;

  for(i = 0; i < 14; i++, c++)
    p = glsl_char_bitmask_to_texture(c, p);

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_CHR_X + (chr % CHARSET_COLS) * CHAR_W,
   TEX_DATA_CHR_Y + (chr / CHARSET_COLS) * CHAR_H,
   CHAR_W,
   CHAR_H,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->charset_texture);
  gl_check_error();
}

static void glsl_update_colors(struct graphics_data *graphics,
 struct rgb_color *palette, unsigned int count)
{
  struct glsl_render_data *render_data = graphics->render_data;
  unsigned int i;
  for(i = 0; i < count; i++)
  {
    graphics->flat_intensity_palette[i] = gl_pack_u32((0xFF << 24) |
     (palette[i].b << 16) | (palette[i].g << 8) | palette[i].r);
    render_data->palette[i*3  ] = (GLubyte)palette[i].r;
    render_data->palette[i*3+1] = (GLubyte)palette[i].g;
    render_data->palette[i*3+2] = (GLubyte)palette[i].b;
  }
}

static void glsl_render_layer(struct graphics_data *graphics,
 struct video_layer *layer)
{
  struct glsl_render_data *render_data = graphics->render_data;
  struct char_element *src = layer->data;
  uint32_t *colorptr;
  uint32_t *dest;
  unsigned int char_value, fg_color, bg_color, subpalette;
  unsigned int i, j;
  int width, height;

  int x1 = layer->x;
  int x2 = layer->x + layer->w * CHAR_W;
  int y1 = layer->y;
  int y2 = layer->y + layer->h * CHAR_H;

  float v_left =   1.0f * x1 / SCREEN_PIX_W_F * 2.0f - 1.0f;
  float v_right =  1.0f * x2 / SCREEN_PIX_W_F * 2.0f - 1.0f;
  float v_top =    1.0f * y1 / SCREEN_PIX_H_F * 2.0f - 1.0f;
  float v_bottom = 1.0f * y2 / SCREEN_PIX_H_F * 2.0f - 1.0f;

  float vertex_array_single[2 * 4] =
  {
    v_left, -v_top,
    v_left, -v_bottom,
    v_right, -v_top,
    v_right, -v_bottom,
  };

  float tex_coord_array_single[2 * 4] =
  {
    0.0f,     0.0f,
    0.0f,     layer->h,
    layer->w, 0.0f,
    layer->w, layer->h,
  };

  if(render_data->use_software_renderer)
  {
    render_layer(render_data->pixels, SCREEN_PIX_W, SCREEN_PIX_H,
     SCREEN_PIX_W * 4, 32, graphics, layer);
    return;
  }

  // Clamp draw area to size of screen texture.
  // TODO: do this elsewhere with proper access to the window struct.
  width = graphics->window.width_px;
  height = graphics->window.height_px;
  if(width < SCREEN_PIX_W || height < SCREEN_PIX_H)
  {
    if(width >= GL_POWER_2_WIDTH)
      width = GL_POWER_2_WIDTH;

    if(height >= GL_POWER_2_HEIGHT)
      height = GL_POWER_2_HEIGHT;
  }
  else
  {
    width = SCREEN_PIX_W;
    height = SCREEN_PIX_H;
  }

  glsl.glViewport(0, 0, width, height);
  gl_check_error();

  if(layer->mode == 0)
  {
    glsl.glUseProgram(render_data->tilemap_program);
    gl_check_error();

    glsl.glUniform1f(render_data->uniform_tilemap_pro_pal,
     graphics->protected_pal_position);
  }
  else
    glsl.glUseProgram(render_data->tilemap_smzx_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  if(render_data->remap_texture)
  {
    glsl_do_remap_charsets(graphics);
    render_data->remap_texture = false;
    memset(render_data->remap_char, false, sizeof(uint8_t) * FULL_CHARSET_SIZE);
  }
  else
  {
    for(i = 0; i < FULL_CHARSET_SIZE; i++)
    {
      if(render_data->remap_char[i])
      {
        glsl_do_remap_char(graphics, i);
        render_data->remap_char[i] = false;
      }
    }
  }

  // Layer data
  dest = render_data->background_texture;

  for(i = 0; i < layer->w * layer->h; i++, dest++, src++)
  {
    // NOTE: leave the bg_color and fg_color in their current form where the
    // protected palette starts at 16. This makes it easier to send data to the
    // shader.
    char_value = src->char_value;
    bg_color = src->bg_color;
    fg_color = src->fg_color;
    subpalette = ((bg_color & 0xF) << 4) | (fg_color & 0xF);

    if(char_value != INVISIBLE_CHAR)
    {
      if(char_value < PROTECTED_CHARSET_POSITION)
        char_value = (char_value + layer->offset) % PROTECTED_CHARSET_POSITION;
    }
    else
    {
      bg_color = PAL_SIZE + PROTECTED_PAL_SIZE;
      fg_color = PAL_SIZE + PROTECTED_PAL_SIZE;
      subpalette = FULL_PAL_SIZE;
    }

    if(layer->mode == 0)
      *dest = gl_pack_u32((char_value << LAYER_CHAR_POS) |
       (bg_color << LAYER_BG_POS) | (fg_color << LAYER_FG_POS));
    else
      *dest = gl_pack_u32((char_value << LAYER_CHAR_POS) |
       (subpalette << LAYER_SUBPALETTE_POS));
  }

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
  gl_check_error();

  glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
   TEX_DATA_LAYER_X, TEX_DATA_LAYER_Y, layer->w, layer->h,
   GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
  gl_check_error();

  // Palette
  if(render_data->dirty_palette ||
   render_data->last_tcol != layer->transparent_col)
  {
    unsigned int transparent = graphics->protected_pal_position + PROTECTED_PAL_SIZE;
    render_data->dirty_palette = false;
    render_data->last_tcol = layer->transparent_col;

    colorptr = graphics->flat_intensity_palette;
    dest = render_data->background_texture;

    for(i = 0; i < graphics->protected_pal_position + PROTECTED_PAL_SIZE; i++)
      dest[i] = colorptr[i];

    if(layer->transparent_col != -1)
      render_data->background_texture[layer->transparent_col] = 0x00000000;
    render_data->background_texture[transparent] = 0x00000000;

    glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
     TEX_DATA_PAL_X, TEX_DATA_PAL_Y, FULL_PAL_SIZE + 1, 1,
     GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
    gl_check_error();
  }

  // Indices
  if(render_data->dirty_indices && layer->mode)
  {
    render_data->dirty_indices = false;
    dest = render_data->background_texture;
    for(i = 0; i < 4; i++)
      for(j = 0; j < SMZX_PAL_SIZE; j++, dest++)
        *dest = gl_pack_u32(graphics->smzx_indices[j * 4 + i]);

    glsl.glTexSubImage2D(GL_TEXTURE_2D, 0,
     TEX_DATA_IDX_X, TEX_DATA_IDX_Y, SMZX_PAL_SIZE, 4,
     GL_RGBA, GL_UNSIGNED_BYTE, render_data->background_texture);
    gl_check_error();
  }

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glEnable(GL_BLEND);
  glsl.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array_single);
  gl_check_error();

  glsl.glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0,
   tex_coord_array_single);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_TEXCOORD);
  glsl.glDisable(GL_BLEND);
}

static void glsl_render_cursor(struct graphics_data *graphics, unsigned int x,
 unsigned int y, uint16_t color, unsigned int lines, unsigned int offset)
{
  struct glsl_render_data *render_data = graphics->render_data;
  GLubyte *pal_base = &render_data->palette[color * 3];

  int x1 = x * 8;
  int x2 = x * 8 + 8;
  int y1 = y * 14 + offset;
  int y2 = y * 14 + offset + lines;

  const float vertex_array[2 * 4] =
  {
    x1 * 2.0f / SCREEN_PIX_W - 1.0f, (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x1 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y1 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
  };

  const float color_array[4 * 4] =
  {
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
    pal_base[0]/255.0f, pal_base[1]/255.0f, pal_base[2]/255.0f,
  };

  if(render_data->use_software_renderer)
  {
    render_cursor(render_data->pixels, SCREEN_PIX_W * 4, 32, x, y,
     graphics->flat_intensity_palette[color], lines, offset);
    return;
  }

  glsl.glUseProgram(render_data->cursor_program);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_COLOR);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array);
  gl_check_error();

  glsl.glVertexAttribPointer(ATTRIB_COLOR, 3, GL_FLOAT, GL_FALSE, 0,
   color_array);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_COLOR);
}

static void glsl_render_mouse(struct graphics_data *graphics,
 unsigned int x, unsigned int y, unsigned int w, unsigned int h)
{
  struct glsl_render_data *render_data = graphics->render_data;

  int x2 = x + w;
  int y2 = y + h;

  const float vertex_array[2 * 4] =
  {
    x  * 2.0f / SCREEN_PIX_W - 1.0f, (y  * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x  * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y  * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
    x2 * 2.0f / SCREEN_PIX_W - 1.0f, (y2 * 2.0f / SCREEN_PIX_H - 1.0f) * -1.0f,
  };

  if(render_data->use_software_renderer)
  {
    render_mouse(render_data->pixels, SCREEN_PIX_W * 4, 32, x, y, 0xFFFFFFFF,
     0x0, w, h);
    return;
  }

  glsl.glEnable(GL_BLEND);
  glsl.glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ZERO);

  glsl.glUseProgram(render_data->mouse_program);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array);
  gl_check_error();

  glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  gl_check_error();

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);

  glsl.glDisable(GL_BLEND);
}

static void glsl_sync_screen(struct graphics_data *graphics,
 struct video_window *window)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int width = window->width_px;
  int height = window->height_px;

  glsl.glViewport(window->viewport_x, window->viewport_y,
   window->viewport_width, window->viewport_height);

  // Clamp draw area to size of screen texture.
  if(width < SCREEN_PIX_W || height < SCREEN_PIX_H)
  {
    if(width >= GL_POWER_2_WIDTH)
      width = GL_POWER_2_WIDTH;

    if(height >= GL_POWER_2_HEIGHT)
      height = GL_POWER_2_HEIGHT;
  }
  else
  {
    width = SCREEN_PIX_W;
    height = SCREEN_PIX_H;
  }

  glsl.glUseProgram(render_data->scaler_program);
  gl_check_error();

  glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_SCREEN_ID]);
  gl_check_error();

  if(render_data->use_software_renderer)
  {
    // Screen data was software rendered to the pixels buffer.
    // Stream it to the screen texture.
    glsl.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SCREEN_PIX_W, SCREEN_PIX_H,
     GL_RGBA, GL_UNSIGNED_BYTE, render_data->pixels);
    width = SCREEN_PIX_W;
    height = SCREEN_PIX_H;
    gl_check_error();
  }
  else

  if(!glsl.has_fbo)
  {
    // If FBOs are enabled, the screen was rendered to the screen texture
    // and nothing extra needs to be done here. If FBOs are not enabled, then
    // the screen needs to be copied to the texture off the window framebuffer.
    glsl.glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 0, 0,
     GL_POWER_2_WIDTH, GL_POWER_2_HEIGHT, 0);
    gl_check_error();
  }

  if(glsl.has_fbo)
  {
    // Select the screen framebuffer to draw the scaled screen to.
    glsl.glBindFramebuffer(GL_FRAMEBUFFER, 0);
    gl_check_error();
  }

  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();

  glsl.glEnableVertexAttribArray(ATTRIB_POSITION);
  glsl.glEnableVertexAttribArray(ATTRIB_TEXCOORD);

  glsl.glVertexAttribPointer(ATTRIB_POSITION, 2, GL_FLOAT, GL_FALSE, 0,
   vertex_array_single);
  gl_check_error();

  {
    const float width_f = width / (1.0f * GL_POWER_2_WIDTH);
    const float height_f = height / (1.0f * GL_POWER_2_HEIGHT);
    const float tex_coord_array_single_hardware[2 * 4] =
    {
      0.0f,     height_f,
      0.0f,     0.0f,
      width_f,  height_f,
      width_f,  0.0f,
    };
    // The software rendered origin is at the top (rather than the bottom).
    const float tex_coord_array_single_software[2 * 4] =
    {
      0.0f,     0.0f,
      0.0f,     height_f,
      width_f,  0.0f,
      width_f,  height_f,
    };
    const float *tex_coord_array_single = render_data->use_software_renderer ?
     tex_coord_array_single_software : tex_coord_array_single_hardware;

    glsl.glVertexAttribPointer(ATTRIB_TEXCOORD, 2, GL_FLOAT, GL_FALSE, 0,
     tex_coord_array_single);
    gl_check_error();

    glsl.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    gl_check_error();
  }

  glsl.glDisableVertexAttribArray(ATTRIB_POSITION);
  glsl.glDisableVertexAttribArray(ATTRIB_TEXCOORD);

  if(!render_data->use_software_renderer)
  {
    glsl.glBindTexture(GL_TEXTURE_2D, render_data->textures[TEX_DATA_ID]);
    gl_check_error();
  }

  gl_swap_buffers(graphics);
  render_data->dirty_palette = true;
  render_data->dirty_indices = true;

  if(glsl.has_fbo)
  {
    /* Switch back to the texture framebuffer. This needs to be done after
     * gl_swap_buffers or the screen won't draw properly for some drivers.
     * The SDL documentation cites macOS, but MegaZeux bugs were encountered
     * using the Intel HD Graphics 2500 driver in Windows too.
     */
    glsl.glBindFramebuffer(GL_FRAMEBUFFER, render_data->fbos[FBO_SCREEN_TEX]);
    gl_check_error();
  }

  /**
   * Clear the framebuffer so elements of this frame don't bleed through.
   * When FBOs are not available, the next window frame needs to be cleared
   * since the screen will be temporarily drawn there before scaling. This call
   * was previously disabled for Emscripten.
   */
  glsl.glClear(GL_COLOR_BUFFER_BIT);
  gl_check_error();
}

static boolean glsl_switch_shader(struct graphics_data *graphics,
 const char *name)
{
  struct glsl_render_data *render_data = graphics->render_data;
  int index = GLSL_SHADER_SCALER_VERT - GLSL_SHADER_RES_FIRST;

  free(source_cache[index]);
  source_cache[index] = NULL;

  index = GLSL_SHADER_SCALER_FRAG - GLSL_SHADER_RES_FIRST;
  free(source_cache[index]);
  source_cache[index] = NULL;

  glsl_set_scaling_shader(graphics, name);

  glsl.glDeleteProgram(render_data->scaler_program);
  gl_check_error();

  /**
   * If the user-specified shader fails to load, this will fall back to the
   * default scaling shader.
   */
  render_data->scaler_program = glsl_load_program(graphics,
   GLSL_SHADER_SCALER_VERT, GLSL_SHADER_SCALER_FRAG);
  if(render_data->scaler_program)
  {
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_POSITION, "Position");
    glsl.glBindAttribLocation(render_data->scaler_program,
     ATTRIB_TEXCOORD, "Texcoord");
    glsl.glLinkProgram(render_data->scaler_program);
    glsl_verify_link(render_data, render_data->scaler_program);
    glsl_delete_shaders(render_data->scaler_program);
  }
  return !glsl_scaling_shader_is_default(graphics);
}

const struct renderer renderer_glsl =
{
  "glsl",
  NULL,
  glsl_init_video,
  glsl_free_video,
  glsl_create_window,
  gl_resize_window,
  glsl_resize_callback,
  set_window_viewport_scaled,
  gl_set_window_caption,
  gl_set_window_icon,
  NULL,
  glsl_update_colors,
  glsl_remap_char_range,
  glsl_remap_char,
  glsl_remap_charbyte,
  glsl_switch_shader,
  NULL,
  glsl_render_layer,
  glsl_render_cursor,
  NULL,
  glsl_render_mouse,
  glsl_sync_screen,
  NULL,
};

const struct renderer renderer_glslscale =
{
  "glslscale",
  NULL,
  glsl_init_video,
  glsl_free_video,
  glsl_software_create_window,
  gl_resize_window,
  glsl_resize_callback,
  set_window_viewport_scaled,
  gl_set_window_caption,
  gl_set_window_icon,
  NULL,
  glsl_update_colors,
  glsl_remap_char_range,
  glsl_remap_char,
  glsl_remap_charbyte,
  glsl_switch_shader,
  NULL,
  glsl_render_layer,
  glsl_render_cursor,
  NULL,
  glsl_render_mouse,
  glsl_sync_screen,
  NULL,
};

const struct renderer renderer_glsl_auto =
{
  "auto_glsl",
  NULL,
  glsl_init_video,
  glsl_free_video,
  glsl_auto_create_window,
  gl_resize_window,
  glsl_resize_callback,
  set_window_viewport_scaled,
  gl_set_window_caption,
  gl_set_window_icon,
  NULL,
  glsl_update_colors,
  glsl_remap_char_range,
  glsl_remap_char,
  glsl_remap_charbyte,
  glsl_switch_shader,
  NULL,
  glsl_render_layer,
  glsl_render_cursor,
  NULL,
  glsl_render_mouse,
  glsl_sync_screen,
  NULL,
};
