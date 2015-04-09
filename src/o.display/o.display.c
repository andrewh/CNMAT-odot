/*
  Written by John MacCallum, The Center for New Music and Audio Technologies,
  University of California, Berkeley.  Copyright (c) 2009-11, The Regents of
  the University of California (Regents). 
  Permission to use, copy, modify, distribute, and distribute modified versions
  of this software and its documentation without fee and without a signed
  licensing agreement, is hereby granted, provided that the above copyright
  notice, this paragraph and the following two paragraphs appear in all copies,
  modifications, and distributions.

  IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
  SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS, ARISING
  OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF REGENTS HAS
  BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

  REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
  PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF ANY, PROVIDED
  HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO OBLIGATION TO PROVIDE
  MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.


  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
  NAME: o.display
  DESCRIPTION: Message box for OSC bundles
  AUTHORS: Ilya Y. Rostovtsev, John MacCallum
  COPYRIGHT_YEARS: 2009-14
  SVN_REVISION: $LastChangedRevision: 587 $
  VERSION 0.0: Inherited from o.message
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@

*/

#define OMAX_DOC_NAME "o.display"
#define OMAX_DOC_SHORT_DESC "Display incoming OSC bundles"
#define OMAX_DOC_LONG_DESC "o.display displays OSC in text form and passes them through to its outlet."
#define OMAX_DOC_INLETS_DESC (char *[]){"An OSC packet is displayed and passed through"}
#define OMAX_DOC_OUTLETS_DESC (char *[]){"OSC FullPacket"}
#define OMAX_DOC_SEEALSO (char *[]){"o.compose"}


#include <string.h>
#include "odot_version.h"


#ifdef OMAX_PD_VERSION
    #include "m_pd.h"
    #include "m_imp.h"
    #include "g_canvas.h"
    #include "g_all_guis.h"
#else
    #include "ext.h"
    #include "ext_obex.h"
    #include "ext_obex_util.h"
    #include "ext_critical.h"
    #include "jpatcher_api.h"
    #include "jgraphics.h"
#endif

#include "omax_util.h"
#include "osc.h"
#include "osc_mem.h"
#include "osc_parser.h"
#include "osc_bundle_u.h"
#include "osc_bundle_s.h"
#include "osc_bundle_iterator_s.h"
#include "osc_bundle_iterator_u.h"
#include "osc_message_iterator_s.h"
#include "osc_message_iterator_u.h"
#include "osc_message_u.h"
#include "osc_message_s.h"
#include "osc_atom_u.h"
#include "osc_atom_s.h"
#include "omax_doc.h"
#include "omax_dict.h"

#include "o.h"

enum {
	odisplay_U,
	odisplay_S,
};

#ifdef OMAX_PD_VERSION
#include "opd_textbox.h"

typedef struct _odisplay {
    t_object ob;
    t_opd_textbox *textbox;
    int         roundpix;
    
    long        textlen;
    char        *tk_tag;

    int         streamflag;
    t_clock     *m_clock;
    
    void *outlet;
    
    //new version
    int newbndl;
	t_osc_bndl_u *bndl_u;
	t_osc_bndl_s *bndl_s;
	int bndl_has_subs;
	int bndl_has_been_checked_for_subs;

    t_opd_rgb *frame_color, *background_color, *text_color, *flash_color;
    
    int have_new_data;
	int draw_new_data_indicator;
	t_clock *new_data_indicator_clock;
    
} t_odisplay;

t_class *odisplay_class;
t_class *odisplay_textbox_class;
t_widgetbehavior odisplay_widgetbehavior;

#else
typedef struct _odisplay{
	t_jbox ob;
	void *outlet;
	t_critical lock;
	int newbndl;
	t_osc_bndl_u *bndl_u;
	t_osc_bndl_s *bndl_s;
//	int bndl_has_subs;
//	int bndl_has_been_checked_for_subs;
	long textlen;
	char *text;
	t_jrgba frame_color, background_color, text_color, flash_color;
	void *qelem;
	int have_new_data;
	int draw_new_data_indicator;
	void *new_data_indicator_clock;
} t_odisplay;

static t_class *odisplay_class;

void odisplay_paint(t_odisplay *x, t_object *patcherview);
void odisplay_mousedown(t_odisplay *x, t_object *patcherview, t_pt pt, long modifiers);
void odisplay_mouseup(t_odisplay *x, t_object *patcherview, t_pt pt, long modifiers);
void odisplay_select(t_odisplay *x);

#endif

void odisplay_doFullPacket(t_odisplay *x, long len, char *ptr);
void odisplay_set(t_odisplay *x, t_symbol *s, long ac, t_atom *av);
void odisplay_doselect(t_odisplay *x);
void odisplay_enter(t_odisplay *x);
void odisplay_gettext(t_odisplay *x);
void odisplay_clear(t_odisplay *x);
void odisplay_clearBundles(t_odisplay *x);
void odisplay_newBundle(t_odisplay *x, t_osc_bndl_u *bu, t_osc_bndl_s *bs);
void odisplay_output_bundle(t_odisplay *x);
void odisplay_bang(t_odisplay *x);
void odisplay_int(t_odisplay *x, long n);
void odisplay_float(t_odisplay *x, double xx);
void odisplay_list(t_odisplay *x, t_symbol *msg, short argc, t_atom *argv);
void odisplay_anything(t_odisplay *x, t_symbol *msg, short argc, t_atom *argv);
void omax_util_outletOSC(void *outlet, long len, char *ptr);
void odisplay_free(t_odisplay *x);
void odisplay_inletinfo(t_odisplay *x, void *b, long index, char *t);
void *odisplay_new(t_symbol *msg, short argc, t_atom *argv);

#ifdef OMAX_PD_VERSION


/* maybe move this to o.h */
#define qelem_set(x)

static int odisplay_click(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit);
void odisplay_drawElements(t_object *ob, int firsttime);

typedef t_odisplay t_jbox;
void jbox_redraw(t_jbox *x){ odisplay_drawElements((t_object *)x, 0);}

#endif

t_symbol *ps_newline, *ps_FullPacket;

void odisplay_fullPacket(t_odisplay *x, t_symbol *msg, int argc, t_atom *argv)
{
	OMAX_UTIL_GET_LEN_AND_PTR
	odisplay_doFullPacket(x, len, ptr);
    odisplay_output_bundle(x);
}

void odisplay_doFullPacket(t_odisplay *x, long len, char *ptr)
{
	osc_bundle_s_wrap_naked_message(len, ptr);
	long copylen = len;
	char *copyptr = osc_mem_alloc(len);
	memcpy(copyptr, ptr, len);
	t_osc_bndl_s *b = osc_bundle_s_alloc(copylen, copyptr);
	odisplay_newBundle(x, NULL, b);
#ifdef OMAX_PD_VERSION
    jbox_redraw((t_jbox *)x);
#else
	qelem_set(x->qelem);
#endif
}

void odisplay_newBundle(t_odisplay *x, t_osc_bndl_u *bu, t_osc_bndl_s *bs)
{
	critical_enter(x->lock);
	odisplay_clearBundles(x);
	x->bndl_u = bu;
	x->bndl_s = bs;
	x->newbndl = 1;
	//x->bndl_has_been_checked_for_subs = 0;
    x->draw_new_data_indicator = 1;
	x->have_new_data = 1;
	critical_exit(x->lock);
}

void odisplay_clearBundles(t_odisplay *x)
{
	critical_enter(x->lock);
	if(x->bndl_u){
		osc_bundle_u_free(x->bndl_u);
		x->bndl_u = NULL;
	}
	if(x->bndl_s){
		osc_bundle_s_deepFree(x->bndl_s);
		x->bndl_s = NULL;
	}
#ifndef OMAX_PD_VERSION
	if(x->text){
		x->textlen = 0;
		osc_mem_free(x->text);
		x->text = NULL;
	}
#endif
	critical_exit(x->lock);
}

void odisplay_output_bundle(t_odisplay *x)
{
	// the use of critical sections is a little weird here, but correct.
	critical_enter(x->lock);
	if(x->bndl_s){
		t_osc_bndl_s *b = x->bndl_s;
		long len = osc_bundle_s_getLen(b);
		char *ptr = osc_bundle_s_getPtr(b);
		char buf[len];
		memcpy(buf, ptr, len);
		critical_exit(x->lock);
		omax_util_outletOSC(x->outlet, len, buf);
        OSC_MEM_INVALIDATE(buf);
		return;
	}
	critical_exit(x->lock);

	char buf[OSC_HEADER_SIZE];
	memset(buf, '\0', OSC_HEADER_SIZE);
	osc_bundle_s_setBundleID(buf);
	omax_util_outletOSC(x->outlet, OSC_HEADER_SIZE, buf);
    OSC_MEM_INVALIDATE(buf);
}

void odisplay_bundle2text(t_odisplay *x)
{
    critical_enter(x->lock);
	if(x->newbndl && x->bndl_s){
		long len = osc_bundle_s_getLen(x->bndl_s);
		char ptr[len];
		memcpy(ptr, osc_bundle_s_getPtr(x->bndl_s), len);
		critical_exit(x->lock);
		long bufpos = osc_bundle_s_nformat(NULL, 0, len, (char *)ptr, 0);
		char *buf = osc_mem_alloc(bufpos + 1);
        osc_bundle_s_nformat(buf, bufpos + 1, len, (char *)ptr, 0);
        if (bufpos != 0) {
		/*
            if(buf[bufpos - 2] == '\n'){
                buf[bufpos - 2] = '\0';
            }
		*/
        } else {
            *buf = '\0';
        }
		
#ifndef OMAX_PD_VERSION
		critical_enter(x->lock);
		if(x->text){
			osc_mem_free(x->text);
		}
		x->textlen = bufpos;
        x->text = buf;
		critical_exit(x->lock);
        object_method(jbox_get_textfield((t_object *)x), gensym("settext"), buf);
#else
        opd_textbox_resetText(x->textbox, buf);

#endif
		if(buf){
			//osc_mem_free(buf);
		}
		x->newbndl = 0;
	}
	critical_exit(x->lock);
}

#ifndef OMAX_PD_VERSION
void odisplay_paint(t_odisplay *x, t_object *patcherview)
{
	int have_new_data = 0;
	int draw_new_data_indicator = 0;
	critical_enter(x->lock);
	have_new_data = x->have_new_data;
	draw_new_data_indicator = x->draw_new_data_indicator;
	critical_exit(x->lock);
	if(have_new_data){	
        odisplay_bundle2text(x);
	}
    
	t_rect rect;
	t_jgraphics *g = (t_jgraphics *)patcherview_get_jgraphics(patcherview);
	jbox_get_rect_for_view((t_object *)x, patcherview, &rect);

	jgraphics_set_source_jrgba(g, &(x->background_color));
    jgraphics_rectangle_rounded(g, 0, 0, rect.width, rect.height - 10, 8, 8);
    jgraphics_rectangle(g, 0, rect.height - 14, 4, 4);
    jgraphics_rectangle(g, rect.width - 4, rect.height - 14, 4, 4);
    jgraphics_fill(g);

    jgraphics_set_source_jrgba(g, &(x->frame_color));
    jgraphics_rectangle_rounded(g, 0, rect.height - 10, rect.width, 10, 8, 8);
    jgraphics_rectangle(g, 0, rect.height - 10, 4, 4);
    jgraphics_rectangle(g, rect.width - 4, rect.height - 10, 4, 4);
    jgraphics_fill(g);
    
    jgraphics_set_source_jrgba(g, &(x->frame_color));
    jgraphics_rectangle_rounded(g, 1, 1, rect.width - 2, rect.height - 2, 8, 8);
    jgraphics_set_line_width(g, 2.);
    jgraphics_stroke(g);
    
    if (draw_new_data_indicator) {
        jgraphics_set_source_jrgba(g, &(x->flash_color));
		jgraphics_ellipse(g, rect.width - 12, 6, 6, 6);
		jgraphics_fill(g);
		critical_enter(x->lock);
		x->draw_new_data_indicator = 0;
		critical_exit(x->lock);
		clock_delay(x->new_data_indicator_clock, 100);
    }
}

#endif

void odisplay_refresh(t_odisplay *x)
{
#ifdef OMAX_PD_VERSION
    x->draw_new_data_indicator = 0;
#endif
	jbox_redraw((t_jbox *)x);
}

#ifndef OMAX_PD_VERSION
void odisplay_select(t_odisplay *x){
	defer(x, (method)odisplay_doselect, 0, 0, 0);
}

void odisplay_doselect(t_odisplay *x){
	t_object *p = NULL; 
	object_obex_lookup(x,gensym("#P"), &p);
	if (p) {
		t_atom rv; 
		long ac = 1; 
		t_atom av[1]; 
		atom_setobj(av, x); 
		object_method_typed(p, gensym("selectbox"), ac, av, &rv); 
	}
}

void odisplay_mousedown(t_odisplay *x, t_object *patcherview, t_pt pt, long modifiers){
    textfield_set_textmargins(jbox_get_textfield((t_object *)x), 6, 6, 5, 15);
	jbox_redraw((t_jbox *)x);
}

void odisplay_mouseup(t_odisplay *x, t_object *patcherview, t_pt pt, long modifiers){
    textfield_set_textmargins(jbox_get_textfield((t_object *)x), 5, 5, 5, 15);
	jbox_redraw((t_jbox *)x);
	odisplay_output_bundle(x);
}
#endif

// we get the text, convert it to an OSC bundle, and then call the paint
// function via qelem_set which converts the OSC bundle back to text.
// We do this so that it's formatted nicely with no unnecessary whitespace
// and tabbed subbundles, etc.
void odisplay_gettext(t_odisplay *x)
{
	long size	= 0;
	char *text	= NULL;
#ifdef OMAX_PD_VERSION
	text = x->textbox->text;
#else
	t_object *textfield = jbox_get_textfield((t_object *)x);
	object_method(textfield, gensym("gettextptr"), &text, &size);
#endif
    
	odisplay_clearBundles(x);
	size = strlen(text); // the value returned in text doesn't make sense
	if(size == 0){
		return;
	}
	char *buf = text;

	if(text[size - 1] != '\n'){
		buf = alloca(size + 2);
		memcpy(buf, text, size);
		buf[size] = '\n';
		buf[size + 1] = '\0';
		size += 2;
	}

	t_osc_bndl_u *bndl_u = NULL;
	t_osc_err e = osc_parser_parseString(size, buf, &bndl_u);
	if(e){
#ifdef OMAX_PD_VERSION
//		x->parse_error = 1;
#endif
		object_error((t_object *)x, "error parsing bundle\n");
		return;
	}
	t_osc_bndl_s *bs = osc_bundle_u_serialize(bndl_u);
	t_osc_bndl_s *bndl_s = NULL;
	osc_bundle_s_deepCopy(&bndl_s, bs);
	odisplay_newBundle(x, bndl_u, bndl_s);
#ifdef OMAX_PD_VERSION
    x->have_new_data = 1;
	jbox_redraw((t_jbox *)x);
#else
	x->have_new_data = 1;
	qelem_set(x->qelem);
#endif
	/*
	if(size > 2){
		int i;
		char *r = text + 1;
		char *w = text + 1;
		char *rm1 = text;
		for(i = 0; i <= size; i++){
			if(*rm1 == ' ' && *r == ' '){
				r++;
			}else{
				*w++ = *r++;
			}
			rm1++;
		}
	}
	*/
}

void odisplay_bang(t_odisplay *x){
//	odisplay_output_bundle(x);
}

void odisplay_int(t_odisplay *x, long n){
	t_atom a;
	atom_setlong(&a, n);
	odisplay_list(x, NULL, 1, &a);
}

void odisplay_float(t_odisplay *x, double f){
	t_atom a;
	atom_setfloat(&a, f);
	odisplay_list(x, NULL, 1, &a);
}

void odisplay_list(t_odisplay *x, t_symbol *list_sym, short argc, t_atom *argv)
{
    object_error((t_object *)x, "o.display doesn't accept non-OSC lists in its inlet");
}

void odisplay_anything(t_odisplay *x, t_symbol *msg, short argc, t_atom *argv)
{
	t_atom av[argc + 1];
	int ac = argc;

	if (msg) {
        ac = argc + 1;
        atom_setsym(av, msg);
        if (argc > 0) {
            memcpy(av + 1, argv, argc * sizeof(t_atom));
        }
	} else {
        memcpy(av, argv, argc * sizeof(t_atom));
	}

    t_osc_msg_u *m = NULL;
    t_osc_err e = omax_util_maxAtomsToOSCMsg_u(&m, msg, argc, argv);
    if(e){
        return;
    }
    t_osc_bndl_u *b = osc_bundle_u_alloc();
    osc_bundle_u_addMsg(b, m);
    t_osc_bndl_s *bs = osc_bundle_u_serialize(b);
    odisplay_newBundle(x, b, bs);

#ifdef OMAX_PD_VERSION
    x->draw_new_data_indicator = 1;
    x->have_new_data = 1;
    jbox_redraw((t_jbox *)x);
#else
    x->draw_new_data_indicator = 1;
    x->have_new_data = 1;
    qelem_set(x->qelem);
#endif
}

void odisplay_set(t_odisplay *x, t_symbol *s, long ac, t_atom *av)
{
    // at present there's nothing to be done here ...
}

void odisplay_clear(t_odisplay *x)
{
    char buf[OSC_HEADER_SIZE];
    memset(buf, '\0', OSC_HEADER_SIZE);
    osc_bundle_s_setBundleID(buf);
    odisplay_doFullPacket(x, OSC_HEADER_SIZE, buf);
}

void odisplay_doc(t_odisplay *x)
{
    omax_doc_outletDoc(x->outlet);
}

/*
    ...........................................................................................
    ................................  PD VERSION  .............................................
    ...........................................................................................

 */
#ifdef OMAX_PD_VERSION

static void odisplay_getrect(t_gobj *z, t_glist *glist,int *xp1, int *yp1, int *xp2, int *yp2)
{
    t_odisplay *x = (t_odisplay *)z;
    int x1, y1, x2, y2;
    
    x1 = text_xpix(&x->ob, glist);
    y1 = text_ypix(&x->ob, glist);
    x2 = x1 + x->textbox->width;
    y2 = y1 + x->textbox->height;
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
    //post("%s %d %d %d %d", __func__, x1, y1, x2, y2);
    opd_textbox_motion(x->textbox);
    
}

void odisplay_drawElements(t_object *ob, int firsttime)
{
    
    t_odisplay *x = (t_odisplay *)ob;
    t_opd_textbox *t = x->textbox;
    
    
    if(!opd_textbox_shouldDraw(t))
        return;
    
    int have_new_data = 0;
    int draw_new_data_indicator = 0;
    critical_enter(x->lock);
    have_new_data = x->have_new_data;
    draw_new_data_indicator = x->draw_new_data_indicator;
    critical_exit(x->lock);
    if(have_new_data){
        odisplay_bundle2text(x);
    }
    
    // odisplay_bundle2text(x);
    
    int x1, y1, x2, y2;
    odisplay_getrect((t_gobj *)x, t->glist, &x1, &y1, &x2, &y2);

    int rpix = x->roundpix;
    int rmargin = rpix;
    int mx1 = x1 - rmargin;
    int mx2 = x2;
    
    int x1a = mx1;
    int y1a = y1 + rpix;
    int x1b = mx1 + rpix;
    int y1b = y1;
    
    int x2a = mx2 - rpix;
    int y2a = y1;
    int x2b = mx2;
    int y2b = y1 + rpix;
    
    int x3a = mx2;
    int y3a = y2 - rpix;
    int x3b = mx2 - rpix;
    int y3b = y2;

    int x4a = mx1 + rpix;
    int y4a = y2;
    int x4b = mx1;
    int y4b = y2 - rpix;
    
    int rx1 = x1 + t->margin_l;
    int ry1 = y1 + t->margin_t;
    int rx2 = x2 - t->margin_r;
    int ry2 = y2 - t->margin_b;
    
    t_glist *glist = t->glist;
    t_canvas *canvas = glist_getcanvas(glist);
    
    
    //    post("%x %s %d %d\n", x, __func__, firsttime, t->firsttime);
    
    if (glist_isvisible(glist) && canvas->gl_editor)
    {
        
        
        if (firsttime)
        {
//              post("%x %s FIRST VIS height %d y1 %d y2 %d \n", x, __func__, t->height, y1, y2);
            
            //box
            sys_vgui(".x%lx.c create polygon %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d -outline %s -width 2 -fill %s -tags %s\n", canvas, x1a, y1a, x1b, y1b, x2a, y2a, x2b, y2b, x3a, y3a, x3b, y3b, x4a, y4a, x4b, y4b, x->frame_color->hex, x->background_color->hex, x->tk_tag);
            sys_vgui(".x%lx.c create polygon %d %d %d %d %d %d %d %d %d %d %d %d -outline \"\" -fill %s -tags %sBOTTOM \n", canvas, mx1, ry2, mx2, ry2, x3a, y3a, x3b, y3b, x4a, y4a, x4b, y4b, x->frame_color->hex, x->tk_tag);
            
            //update dot
            sys_vgui(".x%lx.c create oval %d %d %d %d -fill %s -outline \"\" -tags %sUPDATE \n", canvas, x2-10, y1+5, x2-5, y1+10, x->background_color->hex, x->tk_tag);
            
        }
        else
        {
            //  post("%x %s REDRAW height %d y1 %d y2 %d \n", x, __func__, t->height, y1, y2);
            
            sys_vgui(".x%lx.c coords %s %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d \n", canvas, x->tk_tag, x1a, y1a, x1b, y1b, x2a, y2a, x2b, y2b, x3a, y3a, x3b, y3b, x4a, y4a, x4b, y4b);
            sys_vgui(".x%lx.c coords %sBOTTOM %d %d %d %d %d %d %d %d %d %d %d %d \n", canvas, x->tk_tag, mx1, ry2, mx2, ry2, x3a, y3a, x3b, y3b, x4a, y4a, x4b, y4b);
            
            //sys_vgui(".x%lx.c coords %s %d %d %d %d %d %d %d %d %d %d %d %d \n",canvas, x->tk_tag, x1, y1, x2, y1, x2, ry2, rx2, ry2, rx2, y2, x1, y2);
        }
        
        opd_textbox_drawElements(x->textbox, x1,  y1,  x2,  y2,  firsttime);
        
        sys_vgui(".x%lx.c itemconfigure %sUPDATE -fill %s \n", canvas, x->tk_tag, (draw_new_data_indicator?  x->flash_color->hex : x->background_color->hex ));
        
        if(draw_new_data_indicator)
            clock_delay(x->new_data_indicator_clock, 100);
        
        /* draw inlets/outlets */
        t_object *ob = pd_checkobject(&x->ob.te_pd);
        if (ob){
            glist_drawiofor(glist, ob, firsttime, t->iolets_tag, x1, y1, x2, y2);
            canvas_fixlinesfor(glist, ob);
        }
        
        if (firsttime) /* raise cords over everything else */
            sys_vgui(".x%lx.c raise cord\n", glist_getcanvas(glist));
        
        

    }
}


static void odisplay_vis(t_gobj *z, t_glist *glist, int vis)
{
    t_odisplay *x = (t_odisplay *)z;
    opd_textbox_vis(x->textbox, glist, vis);
}

static void odisplay_displace(t_gobj *z, t_glist *glist,int dx, int dy)
{
    
    t_odisplay *x = (t_odisplay *)z;
    
    if(!x->textbox->mouseDown)
    {
        x->ob.te_xpix += dx;
        x->ob.te_ypix += dy;
        
        t_canvas *canvas = glist_getcanvas(glist);
        
        sys_vgui(".x%lx.c move %s %d %d\n", canvas, x->tk_tag, dx, dy);
        sys_vgui(".x%lx.c move %sUPDATE %d %d\n", canvas, x->tk_tag, dx, dy);
        sys_vgui(".x%lx.c move %sBOTTOM %d %d\n", canvas, x->tk_tag, dx, dy);
        
        opd_textbox_displace(x->textbox, glist, dx, dy);
    }
}

static void odisplay_select(t_gobj *z, t_glist *glist, int state)
{
    //post("%s %d", __func__, state);
    t_odisplay *x = (t_odisplay *)z;
    t_canvas *canvas = glist_getcanvas(glist);
    
    opd_textbox_select(x->textbox, glist, state);
    
    if (glist_isvisible(glist) && gobj_shouldvis(&x->ob.te_g, glist)){
        
        sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", canvas, x->tk_tag, (state? "#006699" : "#0066CC"));
//        sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", canvas, x->corner_tag, (state? "#006699" : "#0066CC"));
        
        sys_vgui(".x%lx.c itemconfigure %sUPDATE -fill %s\n", canvas, x->tk_tag, (x->draw_new_data_indicator? (state? "#006699" : "#0066CC") : x->background_color->hex));
    }
}

static void odisplay_activate(t_gobj *z, t_glist *glist, int state)
{
    //post("%s %d", __func__, state);
    return;
    /*
    t_odisplay *x = (t_odisplay *)z;
    t_canvas *canvas = glist_getcanvas(glist);
    
    if(!state)
        odisplay_gettext(x);
    
    opd_textbox_activate(x->textbox, glist, state);
    
    sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", canvas, x->tk_tag, (state? "#006699" : "#0066CC"));
//    sys_vgui(".x%lx.c itemconfigure %s -outline %s\n", canvas, x->corner_tag, (state? "#006699" : "#0066CC"));
    
    sys_vgui(".x%lx.c itemconfigure %sUPDATE -fill %s\n", canvas, x->tk_tag, (x->draw_new_data_indicator? (state? "#006699" : "#0066CC") : "grey"));
    */
}

static void odisplay_delete(t_gobj *z, t_glist *glist)
{
    //post("%s", __func__);
    t_odisplay *x = (t_odisplay *)z;
    t_opd_textbox *t = x->textbox;
    t_canvas *canvas = glist_getcanvas(glist);
    t_object *ob = pd_checkobject(&x->ob.te_pd);

    //post("%x %s %d", x, __func__, canvas->gl_editor);
    
    if(!t->firsttime && canvas->gl_editor)
    {
        //        opd_textbox_nofocus_callback(t);
        
        sys_vgui(".x%lx.c delete %s\n", canvas, x->tk_tag);
        sys_vgui(".x%lx.c delete %sUPDATE\n", canvas, x->tk_tag);
        sys_vgui(".x%lx.c delete %sBOTTOM\n", canvas, x->tk_tag);
        
        opd_textbox_delete(t, glist);
    }
    
    if(ob && !t->firsttime && glist_isvisible(glist))
    {
        glist_eraseiofor(glist, ob, t->iolets_tag);
    }

    canvas_deletelinesfor(canvas, ob);
}

/*
static void odisplay_doClick(t_odisplay *x,
                             t_floatarg xpos, t_floatarg ypos, t_floatarg shift,
                             t_floatarg ctrl, t_floatarg alt)
{
    if (glist_isvisible(x->textbox->glist))
    {
        sys_vgui(".x%lx.c itemconfigure %sUPDATE -fill %s \n", glist_getcanvas(x->textbox->glist), x->tk_tag, "#0066CC");
        
        odisplay_bang(x);
        clock_delay(x->m_clock, 120);
    }
}
*/

static int odisplay_click(t_gobj *z, struct _glist *glist,
                          int xpix, int ypix, int shift, int alt, int dbl, int doit)
{
    /*
    t_odisplay *x = (t_odisplay *)z;
    {
        if(doit)
        {
            odisplay_doClick(x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, (t_floatarg)0, (t_floatarg)alt);
        }
        return (1);
    }*/
}


static void odisplay_tick(t_odisplay *x)
{
    /*
    if (glist_isvisible(x->textbox->glist))
    {
        sys_vgui(".x%lx.c itemconfigure %s -fill \"white\" \n", glist_getcanvas(x->textbox->glist), x->corner_tag);
    }*/
}


static void odisplay_save(t_gobj *z, t_binbuf *b)
{
    t_odisplay *x = (t_odisplay *)z;
    t_opd_textbox *t = x->textbox;
    
    binbuf_addv(b, "ssiisii", gensym("#X"),gensym("obj"),(t_int)x->ob.te_xpix, (t_int)x->ob.te_ypix, gensym("o.display"), t->width, t->height);
    binbuf_addsemi(b);
}

void odisplay_free(t_odisplay *x)
{
    //post("%x %s", x, __func__);
    free(x->tk_tag);
    free(x->frame_color);
    free(x->background_color);
    free(x->flash_color);
    free(x->text_color);
    
    clock_free(x->m_clock);
    clock_free(x->new_data_indicator_clock);
    
    critical_free(x->lock);
    
    odisplay_clearBundles(x);
    
    opd_textbox_free(x->textbox);
}


void *odisplay_new(t_symbol *msg, short argc, t_atom *argv)
{
    t_odisplay *x = (t_odisplay *)pd_new(odisplay_class);
    if(x)
    {
        
        t_opd_textbox *t = opd_textbox_new(odisplay_textbox_class);
        
        t->glist = (t_glist *)canvas_getcurrent();;
        t->in_new_flag = 1;
        t->firsttime = 1;
        t->parent = (t_object *)x;
        
        t->draw_fn = (t_gotfn)odisplay_drawElements;
        t->gettext_fn = (t_gotfn)odisplay_gettext;
        t->click_fn = (t_gotfn)odisplay_click;
        t->delete_fn = (t_gotfn)odisplay_delete;
        
        t->mouseDown = 0;
        t->selected = 0;
        t->editmode = glist_getcanvas(t->glist)->gl_edit;
        t->textediting = 0;
        
        t->margin_t = 1;
        t->margin_l = 1;
        t->margin_b = 10;
        t->margin_r = 1;
        
        x->roundpix = 3;
        
        t->resizebox_x_offset = 5;
        t->resizebox_y_offset = 5;
        t->resizebox_height = 10;
        t->resizebox_width = 10;
        
        x->textbox = t;
        
        x->frame_color = (t_opd_rgb *)malloc( sizeof(t_opd_rgb) );
        x->background_color = (t_opd_rgb *)malloc( sizeof(t_opd_rgb) );
        x->flash_color = (t_opd_rgb *)malloc( sizeof(t_opd_rgb) );
        x->text_color = (t_opd_rgb *)malloc( sizeof(t_opd_rgb) );

        
        opd_textbox_fsetRGB(x->frame_color, .216, .435, .7137);
        opd_textbox_fsetRGB(x->background_color, .884, .884, .884);
        opd_textbox_fsetRGB(x->flash_color, .761, .349, .306);
        opd_textbox_setRGB(x->text_color, 0, 0, 0);
        // post("%s %p glist %x canvas %x\n", __func__, x, t->glist, glist_getcanvas(t->glist));
        
        x->outlet = outlet_new(&x->ob, NULL);
        
        x->bndl_u = NULL;
        x->bndl_s = NULL;
        x->newbndl = 0;
        x->textlen = 0;
        
        critical_new(&(x->lock));
        
        x->m_clock = clock_new(x, (t_method)odisplay_tick);
        
        x->new_data_indicator_clock = clock_new(x, (t_method)odisplay_refresh);
        x->have_new_data = 1;
        x->draw_new_data_indicator = 0;
        
        x->tk_tag = NULL;
        
        //object name heirarchy:
        char buf[MAXPDSTRING];
        
        sprintf(buf, "%lxBORDER", (long unsigned int)x);
        x->tk_tag = (char *)malloc(sizeof(char) * (strlen(buf)+1));
        if(x->tk_tag == NULL)
        {
            printf("out of memory %d\n", __LINE__);
            return NULL;
        }
        strcpy(x->tk_tag, buf);
        
        opd_textbox_processArgs(t, argc, argv);

        t->in_new_flag = 0;
        t->softlock = 0;
        
    }
    return (void *)x;
}

void setup_o0x2edisplay(void) {
    
    t_class *c = class_new(gensym("o.display"), (t_newmethod)odisplay_new, (t_method)odisplay_free, sizeof(t_odisplay),  0L, A_GIMME, 0);

    class_addmethod(c, (t_method)odisplay_fullPacket, gensym("FullPacket"), A_GIMME, 0);
    class_addmethod(c, (t_method)odot_version, gensym("version"), 0);
    class_addbang(c, (t_method)odisplay_bang);
	class_addfloat(c, (t_method)odisplay_float);
	class_addlist(c, (t_method)odisplay_list);
	class_addanything(c, (t_method)odisplay_anything);
    
//	class_addmethod(c, (t_method)odisplay_set, gensym("set"),0);
    class_addmethod(c, (t_method)odisplay_doc, gensym("doc"),0);

    
    odisplay_widgetbehavior.w_getrectfn = odisplay_getrect;
    odisplay_widgetbehavior.w_displacefn = odisplay_displace;
    odisplay_widgetbehavior.w_selectfn = odisplay_select;
    odisplay_widgetbehavior.w_deletefn = odisplay_delete;
    odisplay_widgetbehavior.w_clickfn = NULL;
    odisplay_widgetbehavior.w_activatefn = NULL;
    odisplay_widgetbehavior.w_visfn = odisplay_vis;
    
    
    class_setsavefn(c, odisplay_save);
    class_setwidget(c, &odisplay_widgetbehavior);
    
    ps_newline = gensym("\n");
	ps_FullPacket = gensym("FullPacket");
    
    odisplay_class = c;
    odisplay_textbox_class = opd_textbox_classnew();
    
    ODOT_PRINT_VERSION;

    //return 0;
    
}

#else

void odisplay_free(t_odisplay *x)
{
    qelem_free(x->qelem);
    object_free(x->new_data_indicator_clock);
    odisplay_clearBundles(x);
	critical_free(x->lock);
    jbox_free((t_jbox *)x);
}

OMAX_DICT_DICTIONARY(t_odisplay, x, odisplay_fullPacket);


void odisplay_assist(t_odisplay *x, void *b, long io, long num, char *buf)
{
	omax_doc_assist(io, num, buf);
}


/**************************************************
 Object and instance creation functions.
 **************************************************/

void *odisplay_new(t_symbol *msg, short argc, t_atom *argv){
	t_odisplay *x;
    
	t_dictionary *d = NULL;
 	long boxflags;
    
	// box setup
	if(!(d = object_dictionaryarg(argc, argv))){
		return NULL;
	}
    
	boxflags = 0
    | JBOX_DRAWFIRSTIN
    | JBOX_NODRAWBOX
    | JBOX_DRAWINLAST
    | JBOX_TRANSPARENT
    //| JBOX_NOGROW
    //| JBOX_GROWY
    //| JBOX_GROWBOTH
    //| JBOX_HILITE
    //| JBOX_BACKGROUND
    //| JBOX_DRAWBACKGROUND
    //| JBOX_NOFLOATINSPECTOR
    //| JBOX_MOUSEDRAGDELTA
    | JBOX_TEXTFIELD
    ;
    
	if((x = (t_odisplay *)object_alloc(odisplay_class))){
		jbox_new((t_jbox *)x, boxflags, argc, argv);
 		x->ob.b_firstin = (void *)x;
		x->outlet = outlet_new(x, NULL);
		//x->proxy = proxy_new(x, 1, &(x->inlet));
		x->bndl_u = NULL;
		x->bndl_s = NULL;
		x->newbndl = 0;
		x->textlen = 0;
		x->text = NULL;
		//x->bndl_has_been_checked_for_subs = 0;
		//x->bndl_has_subs = 0;
		critical_new(&(x->lock));
		x->qelem = qelem_new((t_object *)x, (method)odisplay_refresh);
		x->new_data_indicator_clock = clock_new((t_object *)x, (method)odisplay_refresh);
		x->have_new_data = 1;
		x->draw_new_data_indicator = 0;
		attr_dictionary_process(x, d);
        
		t_object *textfield = jbox_get_textfield((t_object *)x);
		if(textfield){
			object_attr_setchar(textfield, gensym("editwhenunlocked"), 0);
            textfield_set_readonly(textfield, '1');
            textfield_set_selectallonedit(textfield, '1');
			textfield_set_textmargins(textfield, 5, 5, 5, 15);
			textfield_set_textcolor(textfield, &(x->text_color));
		}
        
 		jbox_ready((t_jbox *)x);
		//odisplay_gettext(x);
        odisplay_clear(x);
		return x;
	}
	return NULL;
}

// CLASS_ATTR_STYLE_LABEL as defined in ext_obex_util.h uses gensym_tr() which isn't included in the SDK causing
// compilation to fail.  This kludge just replaces gensym_tr() with gensym()
/*
 #define CLASS_ATTR_STYLE_LABEL_KLUDGE(c,attrname,flags,stylestr,labelstr) \
 { CLASS_ATTR_ATTR_PARSE(c,attrname,"style",USESYM(symbol),flags,stylestr); CLASS_ATTR_ATTR_FORMAT(c,attrname,"label",USESYM(symbol),flags,"s",gensym(labelstr)); }
 */
#define CLASS_ATTR_CATEGORY_KLUDGE(c,attrname,flags,parsestr) \
CLASS_ATTR_ATTR_PARSE(c,attrname,"category",USESYM(symbol),flags,parsestr)


int main(void){
	common_symbols_init();
	t_class *c = class_new("o.display", (method)odisplay_new, (method)odisplay_free, sizeof(t_odisplay), 0L, A_GIMME, 0);
	alias("o.d");
    
	c->c_flags |= CLASS_FLAG_NEWDICTIONARY;
 	jbox_initclass(c, JBOX_TEXTFIELD | JBOX_FIXWIDTH | JBOX_FONTATTR);
    
	class_addmethod(c, (method)odisplay_paint, "paint", A_CANT, 0);
    
//	class_addmethod(c, (method)odisplay_bang, "bang", 0);
//	class_addmethod(c, (method)odisplay_int, "int", A_LONG, 0);
//	class_addmethod(c, (method)odisplay_float, "float", A_FLOAT, 0);
//	class_addmethod(c, (method)odisplay_list, "list", A_GIMME, 0);
//	class_addmethod(c, (method)odisplay_anything, "anything", A_GIMME, 0);
//	class_addmethod(c, (method)odisplay_set, "set", A_GIMME, 0);
	class_addmethod(c, (method)odisplay_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)odisplay_doc, "doc", 0);
	class_addmethod(c, (method)odisplay_fullPacket, "FullPacket", A_GIMME, 0);

	// remove this if statement when we stop supporting Max 5
	if(omax_dict_resolveDictStubs()){
		class_addmethod(c, (method)omax_dict_dictionary, "dictionary", A_GIMME, 0);
	}
    
//	class_addmethod(c, (method)odisplay_clear, "clear", 0);
	class_addmethod(c, (method)odisplay_select, "select", 0);
//	class_addmethod(c, (method)odisplay_mousedown, "mousedown", A_CANT, 0);
//	class_addmethod(c, (method)odisplay_mouseup, "mouseup", A_CANT, 0);
	class_addmethod(c, (method)odot_version, "version", 0);
    
    
 	CLASS_ATTR_RGBA(c, "background_color", 0, t_odisplay, background_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "background_color", 0, ".884 .884 .884 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "background_color", 0, "rgba", "Background Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "background_color", 0, "Color");
    
 	CLASS_ATTR_RGBA(c, "frame_color", 0, t_odisplay, frame_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "frame_color", 0, ".216 .435 .7137 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "frame_color", 0, "rgba", "Frame Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "frame_color", 0, "Color");
    
    CLASS_ATTR_RGBA(c, "flash_color", 0, t_odisplay, flash_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "flash_color", 0, ".761 .349 .306 1.");
 	CLASS_ATTR_STYLE_LABEL(c, "flash_color", 0, "rgba", "Flash Color");
	CLASS_ATTR_CATEGORY_KLUDGE(c, "flash_color", 0, "Color");
    
 	CLASS_ATTR_RGBA(c, "text_color", 0, t_odisplay, text_color);
 	CLASS_ATTR_DEFAULT_SAVE_PAINT(c, "text_color", 0, "0. 0. 0. 1.");
    CLASS_ATTR_DEFAULT(c, "fontname", 0, "\"Courier New\"");
 	//CLASS_ATTR_STYLE_LABEL(c, "text_color", 0, "rgba", "Text Color"); /* this line & next make two Text Color fields in the inspector - remove them for justice */
	//CLASS_ATTR_CATEGORY_KLUDGE(c, "text_color", 0, "Color");
    
	CLASS_ATTR_DEFAULT(c, "rect", 0, "0. 0. 150. 18.");
    
	class_register(CLASS_BOX, c);
	odisplay_class = c;
    
	ps_newline = gensym("\n");
	ps_FullPacket = gensym("FullPacket");
    
	ODOT_PRINT_VERSION;
    
	return 0;
}

#endif


/*  PD NOTES

need to not do any binding if canvas is not visible (in subpatcher), it seems to be happening, and I'm not sure why
**update: check status on that
 
new: duplication seems to alwasy be off by 10px not sure where that is yet
 
 
*/
