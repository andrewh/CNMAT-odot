/*
  Written by John MacCallum, The Center for New Music and Audio Technologies,
  University of California, Berkeley.  Copyright (c) 2011, The Regents of
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
  NAME: o.expr
  DESCRIPTION: C-like expressions that operate on OSC bundles
  AUTHORS: John MacCallum
  COPYRIGHT_YEARS: 2011
  SVN_REVISION: $LastChangedRevision: 587 $
  VERSION 0.0: First try
  VERSION 1.0: Uses flex and bison to do the lexing/parsing
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/

#ifdef NAME
#undef NAME
#endif

#if !defined(OCOND) && !defined(OWHEN) && !defined(OIF) && !defined(OUNLESS)

#define OMAX_DOC_NAME "o.expr"
#define OMAX_DOC_SHORT_DESC "Evaluate a C-like expression containing OSC addresses"
#define OMAX_DOC_LONG_DESC "When it reveives a packet, o.expr substitutes any OSC addresses contained in the expression for the values to which they are bound in the incoming packet.  The expression is then evaluated and the resulting bundle, containing any side effects of the expression, is output."
#define OMAX_DOC_INLETS_DESC (char *[]){"OSC packet containing addresses that the expression will be applied to"}
#define OMAX_DOC_OUTLETS_DESC (char *[]){"The OSC packet containing the results of the expression"}
#define OMAX_DOC_SEEALSO (char *[]){"o.expr.codebox", "o.if", "o.cond", "o.when", "o.unless", "expr", "jit.expr"}
#endif

#define NAME OMAX_DOC_NAME

#if defined OCOND || defined OIF
#define LEFTOUTLET x->outlets[0]
#else
#define LEFTOUTLET x->outlet
#endif

#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_time.h>
#endif

#ifdef OMAX_PD_VERSION
#include "m_pd.h"
#else
#include "ext.h"
#include "ext_obex.h"
#include "ext_obex_util.h"
#include "ext_critical.h"
#endif

//#include "jpatcher_api.h"
//#include "jgraphics.h"
#include "osc.h"
#include "osc_expr.h"
#include "osc_expr_parser.h"
#include "osc_mem.h"
#include "osc_atom_u.h"
#include "osc_error.h"
#include "omax_util.h"
#include "omax_doc.h"
#include "omax_dict.h"
#include "osc_bundle_iterator_s.h"
#include "osc_message_iterator_s.h"

#include "odot_version.h"


//#define __OSC_PROFILE__
#include "osc_profile.h"

#include "o.h"

typedef struct _oexpr{
	t_object ob;
#if defined (OIF) || defined (OCOND)
	void **outlets;
#else
	void *outlet;
#endif
#ifdef OCOND
	int num_exprs;
	char **outlets_desc;
#endif
	t_osc_expr *expr;
} t_oexpr;

void *oexpr_class;

void oexpr_output_bundle(t_oexpr *x);



//void oexpr_fullPacket(t_oexpr *x, long len, long ptr)
void oexpr_fullPacket(t_oexpr *x, t_symbol *msg, int argc, t_atom *argv)
{
	OMAX_UTIL_GET_LEN_AND_PTR
	if(len <= 0){
		return;
	}
#if defined (OIF) || defined (OCOND) || defined (OWHEN) || defined (OUNLESS)
	t_osc_atom_ar_u *av = NULL;
	// we don't actually want to do this copy here.  we need to 
	// have another version of omax_expr_eval that doesn't do 
	// assignment

	char *copy = NULL;
	long copylen = 0;
	char alloc = 0;
	if(strncmp(ptr, "#bundle\0", 8)){
		osc_bundle_s_wrapMessage(len, ptr, &copylen, &copy, &alloc);
	}else{
		copy = (char *)osc_mem_alloc(len);
		memcpy(copy, ptr, len);
	}

#if defined (OIF)
	int ret = osc_expr_eval(x->expr, &len, &copy, &av, x);
	if(ret || !av || osc_atom_array_u_getLen(av) == 0){
		omax_util_outletOSC(x->outlets[1], len, ptr);
	}else{
		int i;
		for(i = 0; i < osc_atom_array_u_getLen(av); i++){
			if(osc_atom_u_getDouble(osc_atom_array_u_get(av, i)) == 0){
				omax_util_outletOSC(x->outlets[1], len, ptr);
				goto out;
			}
		}
		omax_util_outletOSC(x->outlets[0], len, ptr);
	}
 out:
	if(av){
		osc_atom_array_u_free(av);
	}
	if(copy){
		osc_mem_free(copy);
	}
#elif defined (OUNLESS)
	int ret = osc_expr_eval(x->expr, &len, &copy, &av, x);
	if(ret || !av || osc_atom_array_u_getLen(av) == 0){
		omax_util_outletOSC(x->outlet, len, ptr);
	}else{
		int i;
		for(i = 0; i < osc_atom_array_u_getLen(av); i++){
			if(osc_atom_u_getDouble(osc_atom_array_u_get(av, i)) == 0){
				omax_util_outletOSC(x->outlet, len, ptr);
				goto out;
			}
		}
	}
 out:
	if(av){
		osc_atom_array_u_free(av);
	}
	if(copy){
		osc_mem_free(copy);
	}
#elif defined (OWHEN)
	int ret = osc_expr_eval(x->expr, &len, &copy, &av, x);
	if(ret || !av || osc_atom_array_u_getLen(av) == 0){
	}else{
		int i;
		for(i = 0; i < osc_atom_array_u_getLen(av); i++){
			if(osc_atom_u_getDouble(osc_atom_array_u_get(av, i)) == 0){
				goto out;
			}
		}
		omax_util_outletOSC(x->outlet, len, ptr);
	}
 out:
	if(av){
		osc_atom_array_u_free(av);
	}
	if(copy){
		osc_mem_free(copy);
	}
#elif defined (OCOND)
	t_osc_expr *f = x->expr;
	int j = 0;
	int success = 0;
	while(f){
		int ret = osc_expr_eval(f, &len, &copy, &av, x);
		if(!ret){
			int i;
			int fail = 0;
			for(i = 0; i < osc_atom_array_u_getLen(av); i++){
				if(osc_atom_u_getDouble(osc_atom_array_u_get(av, i)) == 0){
					fail = 1;
					break;
				}
			}
			if(av){
				osc_atom_array_u_free(av);
				av = NULL;
			}
			if(!fail){
				omax_util_outletOSC(x->outlets[j], len, ptr);
				//success++;
				goto out;
			}
		}
		f = osc_expr_next(f);
		j++;
	}
	//if(!success){
	omax_util_outletOSC(x->outlets[j], len, ptr);
		//}

 out:
	if(av){
		osc_atom_array_u_free(av);
	}
	if(copy){
		osc_mem_free(copy);
	}
#endif

#else
	// o.expr

	// we need to make a copy incase the expression contains assignment that will
	// alter the bundle.
	// the copy needs to use memory allocated with osc_mem_alloc in case the 
	// bundle has to be resized during assignment
	char *copy = NULL;
	long copylen = len;
	if(strncmp(ptr, "#bundle\0", 8)){
		char alloc = 0;
		osc_bundle_s_wrapMessage(len, ptr, &copylen, &copy, &alloc);
	}else{
		copy = (char *)osc_mem_alloc(len);
		memcpy(copy, ptr, len);
	}
	int ret = 0;
	t_osc_expr *f = x->expr;
	if(!f){
		//t_osc_atom_ar_u *av = NULL;
		//osc_expr_evalLexExprsInBndl(&copylen, &copy, &av);
	}else{
		while(f){
			//int argc = 0;
			t_osc_atom_ar_u *av = NULL;
			ret = osc_expr_eval(f, &copylen, &copy, &av, x);
			if(av){
				osc_atom_array_u_free(av);
			}
			if(ret){
				break;
			}
			f = osc_expr_next(f);
		}
	}
	if(ret){
		omax_util_outletOSC(x->outlet, len, ptr);
	}else{
		omax_util_outletOSC(x->outlet, copylen, copy);
	}
	if(copy){
		osc_mem_free(copy);
	}
#endif
}

void oexpr_postExprAST(t_oexpr *fg)
{
	char *buf = NULL;
	long len = 0;
	t_osc_expr *f = fg->expr;
	//while(f){
	osc_expr_format(f, &len, &buf);
	// the modulo op '%' gets consumed as a format character with cycling's post() function.
	// so go through and escape each one with another %
	char buf2[len * 2];
	char *r = buf, *w = buf2;
	int i;
	for(i = 0; i < len; i++){
		if(*r == '%'){
			*w++ = '%';
		}
		*w++ = *r++;
	}
	*w = '\0';
	post("%s", buf2);
	//f = osc_expr_next(f);
	//
	if(buf){
		osc_mem_free(buf);
	}
}

void oexpr_postFunctionTable(t_oexpr *fg)
{
	char *buf = NULL;
	long len = 0;
	osc_expr_formatFunctionTable(&len, &buf);
	char *ptr1 = buf, *ptr2 = buf;
	while(*ptr2){
		if(*ptr2 == '\n'){
			*ptr2 = '\0';
			post("%s", ptr1);
			ptr1 = ptr2 + 1;
			ptr2++;
		}
		ptr2++;
	}
	if(buf){
		osc_mem_free(buf);
	}
}

void oexpr_bang(t_oexpr *x)
{
	char buf[16];
	memset(buf, '\0', 16);
	strncpy(buf, "#bundle\0", 8);
	//oexpr_fullPacket(x, 16, (long)buf);
#ifdef OMAX_PD_VERSION
    t_atom a[3];
    omax_util_oscLenAndPtr2Atoms(a, 16, buf);
    oexpr_fullPacket(x, NULL, 3, a);
#else
	t_atom a[2];
	atom_setlong(a, 16);
	atom_setlong(a + 1, (long)buf);
	oexpr_fullPacket(x, NULL, 2, a);
#endif
}

#ifndef OMAX_PD_VERSION
OMAX_DICT_DICTIONARY(t_oexpr, x, oexpr_fullPacket);
#endif

void oexpr_doc_cat(t_oexpr *x, t_symbol *msg, int argc, t_atom *argv)
{
	if(argc == 0){
		t_osc_bndl_s *b = osc_expr_getCategories();
		omax_util_outletOSC(LEFTOUTLET, osc_bundle_s_getLen(b), osc_bundle_s_getPtr(b));
	}else{
		if(atom_gettype(argv) != A_SYM){
			object_error((t_object *)x, "doc-cat: argument must be a symbol");
			return;
		}
		t_symbol *cat = atom_getsym(argv);
		long len = 0;
		char *ptr = NULL;
		osc_expr_getFunctionsForCategory(cat->s_name, &len, &ptr);
		if(ptr){
			omax_util_outletOSC(LEFTOUTLET, len, ptr);
			osc_mem_free(ptr);
		}
	}
}

void oexpr_doc_func(t_oexpr *x, t_symbol *msg, int argc, t_atom *argv)
{
	if(argc != 1){
		object_error((t_object *)x, "doc-func expected an argument--the name of a function");
		return;
	}
	if(atom_gettype(argv) != A_SYM){
		object_error((t_object *)x, "doc-func expected the argument to be a symbol--the name of a function");
		return;
	}
	t_symbol *func = atom_getsym(argv);
	t_osc_bndl_u *bndl = NULL;
	t_osc_err e = osc_expr_getDocForFunction(func->s_name, &bndl);
	if(e){
		object_error((t_object *)x, "%s", osc_error_string(e));
		return;
	}
	if(bndl){
		t_osc_bndl_s *bs = osc_bundle_u_serialize(bndl);
		if(bs){
			omax_util_outletOSC(LEFTOUTLET, osc_bundle_s_getLen(bs), osc_bundle_s_getPtr(bs));
			osc_bundle_s_deepFree(bs);
		}
		osc_bundle_u_free(bndl);
	}
}

void oexpr_doc(t_oexpr *x)
{
#ifdef OCOND

	_omax_doc_outletDoc(x->outlets[0],
			    OMAX_DOC_NAME,		
			    OMAX_DOC_SHORT_DESC,	
			    OMAX_DOC_LONG_DESC,		
			    OMAX_DOC_NINLETS,		
			    OMAX_DOC_INLETS_DESC,	
			    x->num_exprs + 1,
			    x->outlets_desc,
			    OMAX_DOC_NUM_SEE_ALSO_REFS,	
			    OMAX_DOC_SEEALSO);
#else
#ifdef OIF
	omax_doc_outletDoc(x->outlets[0]);
#else
	omax_doc_outletDoc(x->outlet);
#endif
#endif
}

#ifndef OMAX_PD_VERSION
void oexpr_assist(t_oexpr *x, void *b, long io, long num, char *buf){
#ifdef OCOND
	_omax_doc_assist(io, num, buf, OMAX_DOC_NINLETS, OMAX_DOC_INLETS_DESC, x->num_exprs + 1, x->outlets_desc);
#else
	omax_doc_assist(io, num, buf);
#endif
}
#endif

void oexpr_free(t_oexpr *x){
	if(x->expr){
		osc_expr_free(x->expr);
	}
#if defined (OIF) || defined (OCOND)
	if(x->outlets){
		free(x->outlets);
	}
#endif
#if defined (OCOND)
	int i;
	for(i = 0; i < x->num_exprs + 1; i++){
		if(x->outlets_desc[i]){
			osc_mem_free(x->outlets_desc[i]);
		}
	}
	osc_mem_free(x->outlets_desc);
#endif
}

#ifndef OMAX_PD_VERSION
t_max_err oexpr_notify(t_oexpr *x, t_symbol *s, t_symbol *msg, void *sender, void *data){
	return MAX_ERR_NONE;
	t_symbol *attrname;

        if(msg == gensym("attr_modified")){
		attrname = (t_symbol *)object_method((t_object *)data, gensym("getname"));
	}
	return MAX_ERR_NONE;
}
#endif

#ifdef OMAX_PD_VERSION
void *oexpr_new(t_symbol *msg, short argc, t_atom *argv){
	t_oexpr *x;
	if((x = (t_oexpr *)object_alloc(oexpr_class))){
		t_osc_expr *f = NULL;
        char symbuf[argc][65536];
		if(argc){
			char buf[65536];
			memset(buf, '\0', sizeof(buf));
			char *ptr = buf;
			int i;
			for(i = 0; i < argc; i++){
				switch(atom_gettype(argv + i)){
                    case A_LONG:
                        ptr += sprintf(ptr, "%ld ", atom_getlong(argv + i));
                        break;
                    case A_FLOAT:
                        ptr += sprintf(ptr, "%f ", atom_getfloat(argv + i));
                        break;
                    case A_SYM:
					{
                        strcpy(symbuf[i], atom_getsymbol(argv + i)->s_name);

                        omax_util_hashBrackets2Curlies(symbuf[i]);
                                                
						char *s = symbuf[i];

                        long len = strlen(s); // null byte

						long j;
						for(j = 0; j < len; j++){  //<< check this for pd version
							if(s[j] == '$'){
								if((j + 1) < len){
									if((s[j + 1] <= 47 || s[j + 1] >= 58)){
										object_error((t_object *)x, "address can't contain a $");
										return NULL;
									}
									ptr += sprintf(ptr, "/_%d_", s[j + 1] - 48);
									j++;
								}else{
									object_error((t_object *)x, "address can't contain a $");
									return NULL;
								}
							}else{
								*ptr++ = s[j];
							}
						}
						*ptr++ = ' ';
					}
                    break;
				}
			}
			if(1){//if(!haspound){
				OSC_PROFILE_TIMER_START(foo);
				t_osc_err ret = osc_expr_parser_parseExpr(buf, &f, x);
				OSC_PROFILE_TIMER_STOP(foo);
				OSC_PROFILE_TIMER_PRINTF(foo);
				OSC_PROFILE_TIMER_SNPRINTF(foo, buff);
 
#ifdef __OSC_PROFILE__
				post("%s\n", buff);
#endif
				if(!f || ret){
					object_error((t_object *)x, "error parsing %s\n", buf);
//					return NULL;  //<< avioding bogus object
                    return NULL;
				} //else {
                    x->expr = f;
                //}
			}else{
				x->expr = NULL;
			}
		}
        
		int n = 0;
		while(f){
			n++;
			f = osc_expr_next(f);
		}
        
#if defined (OIF)
		if(n == 0 || n > 1){
			object_error((t_object *)x, "invalid number of expressions: %d", n);
			return NULL;
		}
		x->outlets = osc_mem_alloc(2 * sizeof(void *));
        x->outlets[0] = outlet_new(&x->ob, gensym("FullPacket"));
		x->outlets[1] = outlet_new(&x->ob, gensym("FullPacket"));
#elif defined (OUNLESS) || defined (OWHEN)
		if(n == 0 || n > 1){
			object_error((t_object *)x, "invalid number of expressions: %d", n);
			return NULL;
		}
		x->outlet = outlet_new(&x->ob, gensym("FullPacket"));
#elif defined (OCOND)
		x->num_exprs = n;
		// implicit 't' as the last condition
		x->outlets = osc_mem_alloc((n + 1) * sizeof(void *));
		int i;
		for(i = 0; i <= n; i++){ 
			x->outlets[i] = outlet_new(&x->ob, gensym("FullPacket"));;
		}
		x->outlets_desc = (char **)osc_mem_alloc((x->num_exprs + 1) * sizeof(char *));
		for(i = 0; i < x->num_exprs; i++){
			x->outlets_desc[i] = (char *)osc_mem_alloc(128);
			sprintf(x->outlets_desc[i], "Input OSC packet if expression %d returns true or non-zero", i+1);
		}
		x->outlets_desc[x->num_exprs] = (char *)osc_mem_alloc(128);
		sprintf(x->outlets_desc[x->num_exprs], "Input OSC packet if all expressions return false or zero");
#else
		x->outlet = outlet_new(&x->ob, gensym("FullPacket"));
#endif
	}
	return x;
}

#if defined (OIF)
int setup_o0x2eif(void)
#elif defined (OUNLESS)
int setup_o0x2eunless(void)
#elif defined (OWHEN)
int setup_o0x2ewhen(void)
#elif defined (OCOND)
int setup_o0x2econd(void)
#else
int setup_o0x2eexpr(void)
#endif
{
	t_class *c = class_new(gensym(NAME), (t_newmethod)oexpr_new, (t_method)oexpr_free, sizeof(t_oexpr), 0L, A_GIMME, 0);
    

	class_addmethod(c, (t_method)oexpr_fullPacket, gensym("FullPacket"), A_GIMME, 0);
//	class_addmethod(c, (t_method)oexpr_assist, gensym("assist"), A_CANT, 0);
	class_addmethod(c, (t_method)oexpr_bang, gensym("bang"), 0);
    
//	class_addmethod(c, (t_method)oexpr_postExprIR, gensym("post-expr-ir"), 0);

    class_addmethod(c, (t_method)oexpr_postExprAST, gensym("post-ast"), 0);

    
	class_addmethod(c, (t_method)oexpr_doc, gensym("doc"), 0);
	class_addmethod(c, (t_method)oexpr_doc_func, gensym("doc-func"), A_GIMME, 0);
	class_addmethod(c, (t_method)oexpr_doc_func, gensym("doc-function"), A_GIMME, 0);
	class_addmethod(c, (t_method)oexpr_doc_cat, gensym("doc-cat"), A_GIMME, 0);
	class_addmethod(c, (t_method)oexpr_doc_cat, gensym("doc-category"), A_GIMME, 0);


    
	class_addmethod(c, (t_method)odot_version, gensym("version"), 0);
    
	oexpr_class = c;
    
//	common_symbols_init();
    
//	osc_error_setHandler(omax_util_liboErrorHandler);
    
	ODOT_PRINT_VERSION;
    
	return 0;
}

#else
void *oexpr_new(t_symbol *msg, short argc, t_atom *argv){
	t_oexpr *x;
	if((x = (t_oexpr *)object_alloc(oexpr_class))){
		t_osc_expr *f = NULL;
		if(argc){
			char buf[65536];
			memset(buf, '\0', sizeof(buf));
			char *ptr = buf;
			int i;
			for(i = 0; i < argc; i++){
				switch(atom_gettype(argv + i)){
				case A_LONG:
					ptr += sprintf(ptr, "%lld ", (long long)atom_getlong(argv + i));
					break;
				case A_FLOAT:
					ptr += sprintf(ptr, "%f ", atom_getfloat(argv + i));
					break;
				case A_SYM:
					{
						char *s = atom_getsym(argv + i)->s_name;
						int len = strlen(s); // null byte
						int j;
						for(j = 0; j < len; j++){
							if(s[j] == '#'){
								if((j + 1) < len){
									if((s[j + 1] <= 47 || s[j + 1] >= 58)){
										object_error((t_object *)x, "address can't contain a #");
										return NULL;
									}
									ptr += sprintf(ptr, "/_%d_", s[j + 1] - 48);
									j++;
								}else{
									object_error((t_object *)x, "address can't contain a #");
									return NULL;
								}
							}else{
								*ptr++ = s[j];
							}
						}
						*ptr++ = ' ';
					}
					break;
				}
			}
			if(1){//if(!haspound){
				OSC_PROFILE_TIMER_START(foo);
				int ret = osc_expr_parser_parseExpr(buf, &f, x);
				OSC_PROFILE_TIMER_STOP(foo);
				OSC_PROFILE_TIMER_PRINTF(foo);
				OSC_PROFILE_TIMER_SNPRINTF(foo, buff);
#ifdef __OSC_PROFILE__
				post("%s\n", buff);
#endif
				if(!f || ret){
					object_error((t_object *)x, "error parsing %s\n", buf);

					return NULL;
				}
				x->expr = f;
			}else{
				x->expr = NULL;
			}
		}

		int n = 0;
		while(f){
			n++;
			f = osc_expr_next(f);
		}

#if defined (OIF)
		if(n == 0 || n > 1){
			object_error((t_object *)x, "invalid number of expressions: %d", n);
			return NULL;
		}
		x->outlets = osc_mem_alloc(2 * sizeof(void *));
		x->outlets[1] = outlet_new((t_object *)x, "FullPacket");
		x->outlets[0] = outlet_new((t_object *)x, "FullPacket");
#elif defined (OUNLESS) || defined (OWHEN)
		if(n == 0 || n > 1){
			object_error((t_object *)x, "invalid number of expressions: %d", n);
			return NULL;
		}
		x->outlet = outlet_new((t_object *)x, "FullPacket");
#elif defined (OCOND)
		x->num_exprs = n;
		// implicit 't' as the last condition
		x->outlets = osc_mem_alloc((n + 1) * sizeof(void *));
		int i;
		for(i = n; i >= 0; i--){
			x->outlets[i] = outlet_new((t_object *)x, "FullPacket");
		}

		x->outlets_desc = (char **)osc_mem_alloc((x->num_exprs + 1) * sizeof(char *));
		for(i = 0; i < x->num_exprs; i++){
			x->outlets_desc[i] = (char *)osc_mem_alloc(128);
			sprintf(x->outlets_desc[i], "Input OSC packet if expression %d returns true or non-zero", i+1);
		}
		x->outlets_desc[x->num_exprs] = (char *)osc_mem_alloc(128);
		sprintf(x->outlets_desc[x->num_exprs], "Input OSC packet if all expressions return false or zero");
#else
		x->outlet = outlet_new((t_object *)x, "FullPacket");
#endif
	}
		   	
	return x;
}

int main(void)
{
	t_class *c = class_new(NAME, (method)oexpr_new, (method)oexpr_free, sizeof(t_oexpr), 0L, A_GIMME, 0);

	//class_addmethod(c, (method)oexpr_fullPacket, "FullPacket", A_LONG, A_LONG, 0);
	class_addmethod(c, (method)oexpr_fullPacket, "FullPacket", A_GIMME, 0);
	class_addmethod(c, (method)oexpr_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)oexpr_bang, "bang", 0);

	class_addmethod(c, (method)oexpr_postExprAST, "post-ast", 0);

	class_addmethod(c, (method)oexpr_doc, "doc", 0);
	class_addmethod(c, (method)oexpr_doc_func, "doc-func", A_GIMME, 0);
	class_addmethod(c, (method)oexpr_doc_func, "doc-function", A_GIMME, 0);
	class_addmethod(c, (method)oexpr_doc_cat, "doc-cat", A_GIMME, 0);
	class_addmethod(c, (method)oexpr_doc_cat, "doc-category", A_GIMME, 0);

	// remove this if statement when we stop supporting Max 5
	//if(omax_dict_resolveDictStubs()){
		class_addmethod(c, (method)omax_dict_dictionary, "dictionary", A_GIMME, 0);
	//}

	class_addmethod(c, (method)odot_version, "version", 0);

	class_register(CLASS_BOX, c);
	oexpr_class = c;

	common_symbols_init();

	osc_error_setHandler(omax_util_liboErrorHandler);

	ODOT_PRINT_VERSION;

	return 0;
}

#endif
