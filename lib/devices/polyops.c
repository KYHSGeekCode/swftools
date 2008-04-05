/* polyops.c

   Part of the swftools package.

   Copyright (c) 2008 Matthias Kramm <kramm@quiss.org> 
 
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <memory.h>
#include <string.h>
#include <math.h>
#include "../mem.h"
#include "../gfxdevice.h"
#include "../gfxtools.h"
#include "../gfxpoly.h"
#include "polyops.h"

typedef struct _clip {
    gfxpoly_t*poly;
    struct _clip*next;
} clip_t;

typedef struct _internal {
    gfxdevice_t*out;
    clip_t*clip;
    gfxpoly_t*polyunion;
} internal_t;

static int verbose = 0;

static void dbg(char*format, ...)
{
    if(!verbose)
	return;
    char buf[1024];
    int l;
    va_list arglist;
    va_start(arglist, format);
    vsprintf(buf, format, arglist);
    va_end(arglist);
    l = strlen(buf);
    while(l && buf[l-1]=='\n') {
	buf[l-1] = 0;
	l--;
    }
    printf("(device-polyops) %s\n", buf);
    fflush(stdout);
}

int polyops_setparameter(struct _gfxdevice*dev, const char*key, const char*value)
{
    dbg("polyops_setparameter");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) return i->out->setparameter(i->out,key,value);
    else return 0;
}

void polyops_startpage(struct _gfxdevice*dev, int width, int height)
{
    dbg("polyops_startpage");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) i->out->startpage(i->out,width,height);
}

void polyops_startclip(struct _gfxdevice*dev, gfxline_t*line)
{
    dbg("polyops_startclip");
    internal_t*i = (internal_t*)dev->internal;

    gfxpoly_t* poly = gfxpoly_fillToPoly(line);

    if(i->clip) {
	gfxpoly_t*old = i->clip->poly;
	clip_t*n = i->clip;
	i->clip = (clip_t*)rfx_calloc(sizeof(clip_t));
	i->clip->next = n;
	i->clip->poly = gfxpoly_intersect(poly, old);
	gfxpoly_free(poly);
    } else {
	i->clip = (clip_t*)rfx_calloc(sizeof(clip_t));
	i->clip->poly = poly;
    }
}

void polyops_endclip(struct _gfxdevice*dev)
{
    dbg("polyops_endclip");
    internal_t*i = (internal_t*)dev->internal;

    if(i->clip) {
	clip_t*old = i->clip;
	i->clip = i->clip->next;
	gfxpoly_free(old->poly);old->poly = 0;
	old->next = 0;free(old);
    } else {
	fprintf(stderr, "Error: endclip without startclip\n");
    }
}

static void addtounion(struct _gfxdevice*dev, gfxpoly_t*poly)
{
    internal_t*i = (internal_t*)dev->internal;
    if(i->polyunion) {
	gfxpoly_t*old = i->polyunion;
	i->polyunion = gfxpoly_union(poly,i->polyunion);
	gfxpoly_free(old);
    }
}

void polyops_stroke(struct _gfxdevice*dev, gfxline_t*line, gfxcoord_t width, gfxcolor_t*color, gfx_capType cap_style, gfx_joinType joint_style, gfxcoord_t miterLimit)
{
    dbg("polyops_stroke");
    internal_t*i = (internal_t*)dev->internal;
    //i->out->stroke(i->out, line, width, color, cap_style, joint_style, miterLimit);
    gfxpoly_t* poly = gfxpoly_strokeToPoly(line, width, cap_style, joint_style, miterLimit);
    if(i->clip) {
	gfxpoly_t*old = poly;
	poly = gfxpoly_intersect(poly, i->clip->poly);
	gfxpoly_free(old);
    }
    addtounion(dev, poly);
    gfxline_t*gfxline = gfxpoly_to_gfxline(poly);
    if(i->out) i->out->fill(i->out, gfxline, color);
    gfxline_free(gfxline);
    gfxpoly_free(poly);
}

void polyops_fill(struct _gfxdevice*dev, gfxline_t*line, gfxcolor_t*color)
{
    dbg("polyops_fill");
    internal_t*i = (internal_t*)dev->internal;

    gfxpoly_t*poly = gfxpoly_fillToPoly(line);

    if(i->clip) {
	gfxpoly_t*old = poly;
	poly  = gfxpoly_intersect(poly, i->clip->poly);
	gfxpoly_free(poly);
    }
    addtounion(dev,poly);
    gfxline_t*gfxline = gfxpoly_to_gfxline(poly);
    if(i->out) i->out->fill(i->out, gfxline, color);
    gfxline_free(gfxline);
    gfxpoly_free(poly);
}

void polyops_fillbitmap(struct _gfxdevice*dev, gfxline_t*line, gfximage_t*img, gfxmatrix_t*matrix, gfxcxform_t*cxform)
{
    dbg("polyops_fillbitmap");
    internal_t*i = (internal_t*)dev->internal;
    gfxpoly_t* poly = gfxpoly_fillToPoly(line);

    if(i->clip) {
	gfxpoly_t*old = poly;
	poly = gfxpoly_intersect(poly, i->clip->poly);
	gfxpoly_free(old);
    }
    addtounion(dev,poly);
    gfxline_t*gfxline = gfxpoly_to_gfxline(poly);
    if(i->out) i->out->fillbitmap(i->out, gfxline, img, matrix, cxform);
    gfxline_free(gfxline);
    gfxpoly_free(poly);
}

void polyops_fillgradient(struct _gfxdevice*dev, gfxline_t*line, gfxgradient_t*gradient, gfxgradienttype_t type, gfxmatrix_t*matrix)
{
    dbg("polyops_fillgradient");
    internal_t*i = (internal_t*)dev->internal;
    gfxpoly_t* poly  = gfxpoly_fillToPoly(line);
    if(i->clip) {
	gfxpoly_t*old = poly;
	poly  = gfxpoly_intersect(poly, i->clip->poly);
	gfxpoly_free(old);
    }
    addtounion(dev,poly);
    gfxline_t*gfxline = gfxpoly_to_gfxline(poly);
    if(i->out) i->out->fillgradient(i->out, gfxline, gradient, type, matrix);
    gfxline_free(gfxline);
    gfxpoly_free(poly);
}

void polyops_addfont(struct _gfxdevice*dev, gfxfont_t*font)
{
    dbg("polyops_addfont");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) i->out->addfont(i->out, font);
}

void polyops_drawchar(struct _gfxdevice*dev, gfxfont_t*font, int glyphnr, gfxcolor_t*color, gfxmatrix_t*matrix)
{
    dbg("polyops_drawchar");
    if(!font)
	return;
    internal_t*i = (internal_t*)dev->internal;
    gfxline_t*glyph = gfxline_clone(font->glyphs[glyphnr].line);
    gfxline_transform(glyph, matrix);

    if(i->clip) {
	gfxbbox_t bbox = gfxline_getbbox(glyph);
	gfxpoly_t*dummybox = gfxpoly_createbox(bbox.xmin,bbox.ymin,bbox.xmax,bbox.ymax);
	gfxpoly_t*poly = gfxpoly_intersect(dummybox, i->clip->poly);
	gfxline_t*gfxline = gfxpoly_to_gfxline(poly);
	gfxbbox_t bbox2 = gfxline_getbbox(gfxline);
	double w = bbox2.xmax - bbox2.xmin;
	double h = bbox2.ymax - bbox2.ymin;
	
	addtounion(dev, poly); // TODO: use the whole char, not just the bbox?

	if(w < 0.001 || h < 0.001) /* character was clipped completely */ {
	} else if(fabs((bbox.xmax - bbox.xmin) - w) > 0.05 ||
                  fabs((bbox.ymax - bbox.ymin) - h) > 0.05) {
	    /* notable change in character size: character was clipped 
	       TODO: handle diagonal cuts
	     */
	    polyops_fill(dev, glyph, color);
	} else {
	    if(i->out) i->out->drawchar(i->out, font, glyphnr, color, matrix);
	}
    } else {
	if(i->out) i->out->drawchar(i->out, font, glyphnr, color, matrix);
    }
    
    gfxline_free(glyph);
}

void polyops_drawlink(struct _gfxdevice*dev, gfxline_t*line, const char*action)
{
    dbg("polyops_drawlink");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) i->out->drawlink(i->out, line, action);
}

void polyops_endpage(struct _gfxdevice*dev)
{
    dbg("polyops_endpage");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) i->out->endpage(i->out);
}

gfxresult_t* polyops_finish(struct _gfxdevice*dev)
{
    dbg("polyops_finish");
    internal_t*i = (internal_t*)dev->internal;
    if(i->out) {
	return i->out->finish(i->out);
    } else {
	return 0;
    }
}

gfxline_t*gfxdevice_union_getunion(struct _gfxdevice*dev)
{
    internal_t*i = (internal_t*)dev->internal;
    return gfxpoly_to_gfxline(i->polyunion);
}

void gfxdevice_removeclippings_init(gfxdevice_t*dev, gfxdevice_t*out)
{
    dbg("gfxdevice_removeclippings_init");
    internal_t*i = (internal_t*)rfx_calloc(sizeof(internal_t));
    memset(dev, 0, sizeof(gfxdevice_t));
    
    dev->name = "removeclippings";

    dev->internal = i;

    dev->setparameter = polyops_setparameter;
    dev->startpage = polyops_startpage;
    dev->startclip = polyops_startclip;
    dev->endclip = polyops_endclip;
    dev->stroke = polyops_stroke;
    dev->fill = polyops_fill;
    dev->fillbitmap = polyops_fillbitmap;
    dev->fillgradient = polyops_fillgradient;
    dev->addfont = polyops_addfont;
    dev->drawchar = polyops_drawchar;
    dev->drawlink = polyops_drawlink;
    dev->endpage = polyops_endpage;
    dev->finish = polyops_finish;

    i->out = out;
    i->polyunion = 0;
}

void gfxdevice_union_init(gfxdevice_t*dev,gfxdevice_t*out)
{
    dbg("gfxdevice_getunion_init");
    internal_t*i = (internal_t*)rfx_calloc(sizeof(internal_t));
    memset(dev, 0, sizeof(gfxdevice_t));
    
    dev->name = "union";

    dev->internal = i;

    dev->setparameter = polyops_setparameter;
    dev->startpage = polyops_startpage;
    dev->startclip = polyops_startclip;
    dev->endclip = polyops_endclip;
    dev->stroke = polyops_stroke;
    dev->fill = polyops_fill;
    dev->fillbitmap = polyops_fillbitmap;
    dev->fillgradient = polyops_fillgradient;
    dev->addfont = polyops_addfont;
    dev->drawchar = polyops_drawchar;
    dev->drawlink = polyops_drawlink;
    dev->endpage = polyops_endpage;
    dev->finish = polyops_finish;

    i->out = out;
    i->polyunion = gfxpoly_strokeToPoly(0, 0, gfx_capButt, gfx_joinMiter, 0);
}
