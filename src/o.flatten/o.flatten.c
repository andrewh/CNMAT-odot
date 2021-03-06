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
  NAME: o.flatten
  DESCRIPTION: Flatten a bundle with nested bundles
  AUTHORS: John MacCallum
  COPYRIGHT_YEARS: 2012
  SVN_REVISION: $LastChangedRevision: 587 $
  VERSION 0.0: First try
  @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@
*/


#define OMAX_DOC_NAME "o.flatten"
#define OMAX_DOC_SHORT_DESC "Flatten the address hierarchy in nested bundles"
#define OMAX_DOC_LONG_DESC "o.flatten takes messages out of nested bundles and inserts them into the top level bundle."
#define OMAX_DOC_INLETS_DESC (char *[]){"OSC packet"}
#define OMAX_DOC_OUTLETS_DESC (char *[]){"The flattened OSC packet"}
#define OMAX_DOC_SEEALSO (char *[]){"o.explode"}

#include "odot_version.h"
#ifdef OMAX_PD_VERSION
#include "m_pd.h"
#else
#include "ext.h"
#include "ext_obex.h"
#include "ext_obex_util.h"
#include "ext_critical.h"
#endif
#include "osc.h"
#include "osc_mem.h"
#include "omax_util.h"
#include "omax_doc.h"
#include "omax_dict.h"

#include "o.h"

typedef struct _oflatten{
	t_object ob;
	void *outlet;
	int level;
	t_symbol *sep;
	int remove_enclosing_address_if_empty;
} t_oflatten;

void *oflatten_class;

int oflatten_copybundle(t_oflatten *x, long len, char *ptr);

void oflatten_fullPacket(t_oflatten *x, t_symbol *msg, int argc, t_atom *argv)
{
	OMAX_UTIL_GET_LEN_AND_PTR
	char srcc[osc_bundle_s_getStructSize()];
	t_osc_bndl_s *src = (t_osc_bndl_s *)srcc;
	osc_bundle_s_setLen(src, len);
	osc_bundle_s_setPtr(src, ptr);
	t_osc_bndl_s *dest = NULL;
	osc_bundle_s_flatten(&dest, src, x->level, x->sep->s_name, x->remove_enclosing_address_if_empty);
	omax_util_outletOSC(x->outlet, osc_bundle_s_getLen(dest), osc_bundle_s_getPtr(dest));
	osc_bundle_s_deepFree(dest);
}

void oflatten_doc(t_oflatten *x)
{
	omax_doc_outletDoc(x->outlet);
}

#ifdef OMAX_PD_VERSION
void *oflatten_new(t_symbol *msg, short argc, t_atom *argv)
{
	t_oflatten *x;
	if((x = (t_oflatten *)object_alloc(oflatten_class))){
		x->outlet = outlet_new((t_object *)x, gensym("FullPacket"));
		x->level = 0;
		x->sep = gensym("");
		x->remove_enclosing_address_if_empty = 1;

/********************* PD Pseudo-attributed, commented out for 1.0 release
 
        int i;
        for(i = 0; i < argc; i++)
        {
            if(atom_gettype(argv + i) == A_SYM)
            {
                t_symbol *attribute = atom_gensym(argv+i);
                if(attribute == gensym("@level")){
                    if(atom_gettype(argv+(++i)) == A_FLOAT)
                    {
                        x->level = atom_getfloat(argv+i);
                    } else {
                        post("@level value must be a number");
                        return 0;
                    }
                } else if(attribute == gensym("@sep")){
                    if(atom_gettype(argv+(++i)) == A_SYMBOL)
                    {
                        x->sep = atom_getsym(argv+i);
                    } else {
                        post("@sep value must be a symbol");
                        return 0;
                    }
                } else if(attribute == gensym("@remove_enclosing_address_if_empty")){
                    if(atom_gettype(argv+(++i)) == A_FLOAT)
                    {
                        x->remove_enclosing_address_if_empty = atom_getfloat(argv+i);
                    } else {
                        post("@remove_enclosing_address_if_empty value must be a number (1 or 0)");
                        return 0;
                    }
                    
                } else if(attribute->s_name[0] == '@') {
                    post("unknown attribute");
                }  else {
                    post("o.explode optional attributes are @level <value> and @sep <value>");
                }
                
            } else {
                post("o.explode optional attributes are @level <value> and @sep <value>");
                return 0;
            }
            
            
        }
*/
	}
    
	return x;
}

int setup_o0x2eflatten(void)
{
	t_class *c = class_new(gensym("o.flatten"), (t_newmethod)oflatten_new, NULL, sizeof(t_oflatten), 0L, A_GIMME, 0);
	class_addmethod(c, (t_method)oflatten_fullPacket, gensym("FullPacket"), A_GIMME, 0);
	class_addmethod(c, (t_method)oflatten_doc, gensym("doc"), 0);

	class_addmethod(c, (t_method)odot_version, gensym("version"), 0);
    
	oflatten_class = c;
    
	ODOT_PRINT_VERSION;
	return 0;
}


#else
OMAX_DICT_DICTIONARY(t_oflatten, x, oflatten_fullPacket);

void oflatten_assist(t_oflatten *x, void *b, long io, long num, char *buf)
{
	omax_doc_assist(io, num, buf);
}

void *oflatten_new(t_symbol *msg, short argc, t_atom *argv)
{
	t_oflatten *x;
	if((x = (t_oflatten *)object_alloc(oflatten_class))){
		x->outlet = outlet_new((t_object *)x, "FullPacket");
		x->level = 0;
		x->sep = gensym("");
		x->remove_enclosing_address_if_empty = 1;
		attr_args_process(x, argc, argv);
	}
		   	
	return x;
}

int main(void)
{
	t_class *c = class_new("o.flatten", (method)oflatten_new, NULL, sizeof(t_oflatten), 0L, A_GIMME, 0);
	//class_addmethod(c, (method)oflatten_fullPacket, "FullPacket", A_LONG, A_LONG, 0);
	class_addmethod(c, (method)oflatten_fullPacket, "FullPacket", A_GIMME, 0);
	class_addmethod(c, (method)oflatten_assist, "assist", A_CANT, 0);
	class_addmethod(c, (method)oflatten_doc, "doc", 0);
	// remove this if statement when we stop supporting Max 5
	//if(omax_dict_resolveDictStubs()){
		class_addmethod(c, (method)omax_dict_dictionary, "dictionary", A_GIMME, 0);
	//}

	class_addmethod(c, (method)odot_version, "version", 0);
	/*
	CLASS_ATTR_LONG(c, "level", 0, t_oflatten, level);
	CLASS_ATTR_SYM(c, "sep", 0, t_oflatten, sep);
	CLASS_ATTR_LONG(c, "remove_enclosing_address_if_empty", 0, t_oflatten, remove_enclosing_address_if_empty);
	*/
	class_register(CLASS_BOX, c);
	oflatten_class = c;

	common_symbols_init();

	ODOT_PRINT_VERSION;
	return 0;
}
#endif
