/* Copyright (C) 2010 Alan Williams <mralert@gmail.com>
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

#include "smzxconv.h"
#include "../io/image_file.h"

typedef struct {
	int r;
	int g;
	int b;
	int c;
} rgba_accum;

typedef struct {
	mzx_glyph g;
	unsigned short w;
	unsigned int d;
} glyph_dist;

struct _smzx_converter {
	int w, h;
	int iw, ih;
	int tsiz, isiz;
	int chroff, chrskip, chrlen;
	int chrprelen;
	int clroff, clrlen;
	struct rgba_color *src;
	unsigned char *dest;
	unsigned char *hist;
	int *lerr;
	glyph_dist *tgly;
	glyph_dist *cset;
};

static int gdist_table[256][256];
static int grdist_table[256][256];

smzx_converter *smzx_convert_init (int w, int h, int chroff, int chrskip,
	int chrlen, int clroff, int clrlen) {
	smzx_converter *c;
	int chrprelen = chrlen;
	int iw = w * 4;
	int ih = h * 14;
	int tsiz = w * h;
	int isiz = iw * ih;
	int setsize = tsiz;
	if ((chrskip > chroff) && (chrskip < chroff + chrlen)) chrprelen--;
	if (setsize < chroff + chrlen) setsize = chroff + chrlen;
	c = malloc(sizeof(smzx_converter));
	if (!c) return NULL;
	c->w = w;
	c->h = h;
	c->iw = iw;
	c->ih = ih;
	c->tsiz = tsiz;
	c->isiz = isiz;
	c->chroff = chroff;
	c->chrskip = chrskip;
	c->chrlen = chrlen;
	c->chrprelen = chrprelen;
	c->clroff = clroff;
	c->clrlen = clrlen;
	c->src = malloc(sizeof(struct rgba_color) * isiz);
	c->dest = malloc(isiz);
	c->hist = malloc(isiz);
	c->lerr = malloc(sizeof(int) * (iw + 2) * 2);
	c->tgly = malloc(sizeof(glyph_dist) * setsize);
	c->cset = malloc(sizeof(glyph_dist) * setsize);
	if (!c->src || !c->dest || !c->hist || !c->tgly || !c->cset) {
		free(c->cset);
		free(c->tgly);
		free(c->hist);
		free(c->dest);
		free(c->src);
		free(c);
		return NULL;
	}
	return c;
}

static int ccmp (const void *av, const void *bv) {
	const unsigned char *a = av;
	const unsigned char *b = bv;
	if (*a < *b) return -1;
	if (*a > *b) return 1;
	return 0;
}

static void init_gdist_tables(void)
{
	static int init = 0;
	const unsigned char swap[4] = {0, 2, 1, 3};
	int x, y;

	if (!init) {
		for (y = 0; y < 4; y++)
			for (x = 0; x < 4; x++)
				gdist_table[x][y] = (swap[x] - swap[y]) * (swap[x] - swap[y]);
		for (y = 0; y < 256; y++) {
			for (x = 0; x < 256; x++) {
				gdist_table[x][y] = gdist_table[x&3][y&3] + gdist_table[(x>>2)&3][(y>>2)&3]
					+ gdist_table[(x>>4)&3][(y>>4)&3] + gdist_table[x>>6][y>>6];
				grdist_table[x][y ^ 0xff] = gdist_table[x][y];
			}
		}
		init = 1;
	}
}

static int gdist (const mzx_glyph a, const mzx_glyph b) {
	int i, res;
	res = 0;
	init_gdist_tables();
	for (i = 0; i < 14; i++)
		res += gdist_table[a[i]][b[i]];
	return res;
}

static int grdist (const mzx_glyph a, const mzx_glyph b) {
	int i, res;
	res = 0;
	init_gdist_tables();
	for (i = 0; i < 14; i++)
		res += grdist_table[a[i]][b[i]];
	return res;
}

static int gpdist (const mzx_glyph a, const mzx_glyph b) {
	int res, rev;
	res = gdist(a, b);
	rev = grdist(a, b);
	return (res < rev) ? res : rev;
}

static int gcmp (const void *av, const void *bv) {
	const mzx_glyph blank = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const glyph_dist *a = av;
	const glyph_dist *b = bv;
	int ad, bd, i;
	if (a->d > b->d) return -1;
	if (a->d < b->d) return 1;
	ad = gdist(a->g, blank);
	bd = gdist(b->g, blank);
	if (ad < bd) return -1;
	if (ad > bd) return 1;
	for (i = 0; i < 14; i++) {
		if (a->g[i] < b->g[i]) return -1;
		if (a->g[i] > b->g[i]) return 1;
	}
	return 0;
}

int smzx_convert (smzx_converter *c, const struct rgba_color *img, mzx_tile *tile,
	mzx_glyph *chr, mzx_color *pal) {
	const mzx_glyph blank = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
	const mzx_glyph full = {255, 255, 255, 255, 255, 255, 255, 255, 255,
		255, 255, 255, 255, 255};
	const struct rgba_color *ipx;
	struct rgba_color *spx;
	unsigned char *dpx, *hpx;
	int *epx;
	unsigned char gpal[16], rpal[256];
	unsigned char lut[256];
	unsigned char bg, fg, tmp;
	rgba_accum apal[256];
	int i, j, u, v, x, y;

	if (!c) return 0;

	memset(apal, 0, sizeof(rgba_accum) * 256);

	/* Half-width copy image */
	for (i = 0, ipx = img, spx = c->src; i < c->isiz;
		i++, ipx += 2, spx++) {
		spx->r = (ipx[0].r * ipx[0].a + ipx[1].r * ipx[1].a) / 2040;
		spx->g = (ipx[0].g * ipx[0].a + ipx[1].g * ipx[1].a) / 2040;
		spx->b = (ipx[0].b * ipx[0].a + ipx[1].b * ipx[1].a) / 2040;
		spx->a = 63;
	}
	/* Grayscale image */
	for (i = 0, spx = c->src, dpx = c->dest; i < c->isiz;
		i++, spx++, dpx++)
		*dpx = (spx->r * 30 + spx->g * 59 + spx->b * 11) * spx->a
			/ 6300;
	/* Create histogram-based grayscale palette */
	memcpy(c->hist, c->dest, c->isiz);
	qsort(c->hist, c->isiz, 1, ccmp);
	for (i = 0; i < c->clrlen; i++)
		gpal[i] = (c->hist[i*(c->isiz-1)/(c->clrlen-1)] * 2
			+ (c->hist[0] * (c->clrlen - 1 - i)
			+ c->hist[c->isiz-1] * i) / (c->clrlen - 1)) / 3;
	memmove(gpal + c->clroff, gpal, c->clrlen);
	/* Find min/max values for each tile and assign tile colors accordingly */
	for (v = 0; v < c->h; v++) {
		for (u = 0; u < c->w; u++) {
			bg = 255;
			fg = 0;
			dpx = c->dest + u * 4 + v * 14 * c->iw;
			for (y = 0; y < 14; y++) {
				for (x = 0; x < 4; x++) {
					tmp = dpx[x+y*c->iw];
					if (bg > tmp) bg = tmp;
					if (fg < tmp) fg = tmp;
				}
			}
			for (i = c->clroff; i < c->clroff + c->clrlen; i++)
				if (gpal[i] <= bg) tmp = i;
			tile[u+v*c->w].clr = tmp << 4;
			for (i = c->clroff + c->clrlen - 1; i >= c->clroff;
				i--)
				if (gpal[i] >= fg) tmp = i;
			tile[u+v*c->w].clr |= tmp;
		}
	}
	/* Create SMZX interpolated palette from tile histograms */
	for (i = c->clroff; i < c->clroff + c->clrlen; i++)
		rpal[(i<<4)|i] = gpal[i];
	for (i = c->clroff; i < c->clroff + c->clrlen - 1; i++) {
		for (j = i + 1; j < c->clroff + c->clrlen; j++) {
			hpx = c->hist;
			for (x = v = 0; v < c->h; v++) for (u = 0; u < c->w; u++, x++) {
				if ((tile[x].clr == ((i << 4) | j))
					|| (tile[x].clr == ((j << 4) | i))) {
					dpx = c->dest + u * 4 + v * 14 * c->iw;
					for (y = 0; y < 14; y++, hpx += 4, dpx += c->iw)
						memcpy(hpx, dpx, 4);
				}
			}
			x = hpx - c->hist;
			if (x) {
				qsort(c->hist, x, 1, ccmp);
				rpal[(j<<4)|i] = (c->hist[(x-1)/3] * 2
					+ (c->hist[0] * 2 + c->hist[x-1]) / 3)
					/ 3;
				rpal[(i<<4)|j] = (c->hist[2*(x-1)/3] * 2
					+ (c->hist[0] + c->hist[x-1] * 2) / 3)
					/ 3;
			} else {
				rpal[(j<<4)|i] = (gpal[i] * 2 + gpal[j]) / 3;
				rpal[(i<<4)|j] = (gpal[i] + gpal[j] * 2) / 3;
			}
		}
	}
	/* Create characters for each tile (w/ Floyd-Steinberg dithering) */
	memset(c->lerr + c->iw + 1, 0, sizeof(int) * c->iw);
	for (y = 0, dpx = c->dest, hpx = c->hist; y < c->ih; y++) {
		v = y / 14;
		memcpy(c->lerr, c->lerr + c->iw + 1, sizeof(int) * c->iw);
		memset(c->lerr + c->iw + 1, 0, sizeof(int) * c->iw);
		for (x = 0, epx = c->lerr; x < c->iw; x++, dpx++, hpx++, epx++) {
			u = x / 4;
			tmp = tile[u+v*c->w].clr;
			bg = tmp >> 4;
			fg = tmp & 15;
			tmp = (rpal[(bg<<4)|bg] + rpal[(fg<<4)|fg]) / 2;
			*epx += *dpx;
			if (*epx >= tmp) {
				tmp = (rpal[(bg<<4)|fg] + rpal[(fg<<4)|fg]) / 2;
				if (*epx >= tmp) *hpx = (fg << 4) | fg;
				else *hpx = (bg << 4) | fg;
			} else {
				tmp = (rpal[(bg<<4)|bg] + rpal[(fg<<4)|bg]) / 2;
				if (*epx >= tmp) *hpx = (fg << 4) | bg;
				else *hpx = (bg << 4) | bg;
			}
			if (gpal[bg] == gpal[fg]) *hpx = (bg << 4) | bg;
			*epx -= rpal[*hpx];
			epx[1] += *epx * 7 / 16;
			epx[c->iw] += *epx * 3 / 16;
			epx[c->iw+1] += *epx * 5 / 16;
			epx[c->iw+2] += *epx - (*epx * 15 / 16);
		}
	}
	/* Convert characters to SMZX format */
	for (v = 0; v < c->h; v++) {
		for (u = 0; u < c->w; u++) {
			hpx = c->hist + u * 4 + v * 14 * c->iw;
			tmp = tile[u+v*c->w].clr;
			bg = tmp >> 4;
			fg = tmp & 15;
			lut[(bg<<4)|bg] = 0;
			lut[(fg<<4)|bg] = 2;
			lut[(bg<<4)|fg] = 1;
			lut[(fg<<4)|fg] = 3;
			for (y = 0; y < 14; y++, hpx += c->iw)
				c->tgly[u+v*c->w].g[y] = (lut[hpx[0]] << 6)
					| (lut[hpx[1]] << 4)
					| (lut[hpx[2]] << 2) | lut[hpx[3]];
			c->tgly[u+v*c->w].w = abs(gpal[fg] - gpal[bg]);
		}
	}
	/* Reduce character set to specified size */
	memcpy(c->cset, c->tgly, sizeof(glyph_dist) * c->tsiz);
	for (i = 0; i < c->tsiz; i++) {
		c->cset[i].d = 32767;
		if (gdist(c->cset[i].g, blank) > gdist(c->cset[i].g, full))
			for (j = 0; j < 14; j++)
				c->cset[i].g[j] ^= 0xFF;
	}
	qsort(c->cset, c->tsiz, sizeof(glyph_dist), gcmp);
	for (i = 0; i < c->tsiz - 1; i++) {
		for (j = i + 1; j < c->tsiz; j++) {
			x = gpdist(c->cset[i].g, c->cset[j].g);
			if ((unsigned int)x < c->cset[j].d) c->cset[j].d = x;
		}
	}
	for (i = 0; i < c->tsiz; i++)
		c->cset[i].d *= c->cset[i].w;
	qsort(c->cset, c->tsiz, sizeof(glyph_dist), gcmp);
	for (i = 0; i < c->chrprelen; i++)
		c->cset[i].d = 0;
	qsort(c->cset, c->chrprelen, sizeof(glyph_dist), gcmp);
	memmove(c->cset + c->chroff, c->cset,
		sizeof(glyph_dist) * c->chrprelen);
	if (c->chrprelen < c->chrlen) {
		memmove(c->cset + c->chrskip + 1, c->cset + c->chrskip,
			sizeof(glyph_dist) * (c->chroff + c->chrprelen
			- c->chrskip));
		memset(c->cset + c->chrskip, 0, sizeof(glyph_dist));
	}
	for (i = 0; i < c->chrlen; i++)
		memcpy(chr[i], c->cset[i+c->chroff].g, sizeof(mzx_glyph));
	/* Map tiles to characters from reduced set */
	for (i = u = 0; i < c->tsiz; i++) {
		x = gdist(c->tgly[i].g, c->cset->g) + 1;
		for (j = c->chroff; j < c->chroff + c->chrlen; j++) {
			if (j == c->chrskip) continue;
			y = gdist(c->tgly[i].g, c->cset[j].g);
			if (y < x) {
				x = y;
				tile[i].chr = j;
				u = 0;
			}
			y = grdist(c->tgly[i].g, c->cset[j].g);
			if (y < x) {
				x = y;
				tile[i].chr = j;
				u = 1;
			}
		}
		if (u) tile[i].clr = (tile[i].clr << 4) | (tile[i].clr >> 4);
	}
	/* Draw resulting grayscale image */
	for (v = 0; v < c->h; v++) {
		for (u = 0; u < c->w; u++) {
			hpx = c->hist + u * 4 + v * 14 * c->iw;
			tmp = tile[u+v*c->w].clr;
			bg = tmp >> 4;
			fg = tmp & 15;
			lut[0] = (bg << 4) | bg;
			lut[2] = (fg << 4) | bg;
			lut[1] = (bg << 4) | fg;
			lut[3] = (fg << 4) | fg;
			for (y = 0; y < 14; y++, hpx += c->iw) {
				hpx[0] = lut[c->cset[tile[u+v*c->w].chr].g[y]>>6];
				hpx[1] = lut[(c->cset[tile[u+v*c->w].chr].g[y]>>4)&3];
				hpx[2] = lut[(c->cset[tile[u+v*c->w].chr].g[y]>>2)&3];
				hpx[3] = lut[c->cset[tile[u+v*c->w].chr].g[y]&3];
			}
		}
	}
	/* Calculate average color of pixels assigned to each index */
	for (i = 0, spx = c->src, dpx = c->dest, hpx = c->hist; i < c->isiz;
		i++, spx++, dpx++, hpx++) {
			apal[*hpx].r += spx->r;
			apal[*hpx].g += spx->g;
			apal[*hpx].b += spx->b;
			apal[*hpx].c += *dpx;
	}
	for (i = 0; i < 256; i++) {
		bg = i >> 4;
		fg = i & 15;
		if (bg < c->clroff) continue;
		if (fg < c->clroff) continue;
		if (bg >= c->clroff + c->clrlen) continue;
		if (fg >= c->clroff + c->clrlen) continue;
		if (apal[i].c) {
			apal[i].r = apal[i].r * rpal[i] / apal[i].c;
			apal[i].g = apal[i].g * rpal[i] / apal[i].c;
			apal[i].b = apal[i].b * rpal[i] / apal[i].c;
			if (apal[i].r > 63) apal[i].r = 63;
			if (apal[i].g > 63) apal[i].g = 63;
			if (apal[i].b > 63) apal[i].b = 63;
		} else
			apal[i].r = apal[i].g = apal[i].b = rpal[i];
		pal[i].r = apal[i].r;
		pal[i].g = apal[i].g;
		pal[i].b = apal[i].b;
	}
	return 1;
}

void smzx_convert_free (smzx_converter *c) {
	if (!c) return;
	free(c->src);
	free(c->dest);
	free(c->hist);
	free(c->tgly);
	free(c->cset);
	free(c);
}
