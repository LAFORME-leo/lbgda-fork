/* GDA common library
 * Copyright (C) 2008 The GNOME Foundation.
 *
 * AUTHORS:
 *      Johannes Schmid
 *      Vivien Malerba <malerba@gnome-db.org>
 *
 * This Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this Library; see the file COPYING.LIB.  If not,
 * write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gda-custom-marshal.h>


/* These marshallers are hardcoded here because glib-genmarshal does not support the marshalled types */
void
_gdaui_marshal_VOID__STRING_VALUE (GClosure     *closure,
				   GValue       *return_value G_GNUC_UNUSED,
				   guint         n_param_values,
				   const GValue *param_values,
				   gpointer      invocation_hint G_GNUC_UNUSED,
				   gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_VALUE) (gpointer     data1,
							 gpointer     arg_1,
							 gpointer     arg_2,
							 gpointer     data2);
	register GMarshalFunc_VOID__STRING_VALUE callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 3);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__STRING_VALUE) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  (gpointer) g_value_get_string (param_values + 1),
		  g_value_get_boxed (param_values + 2),
		  data2);
}

void
_gdaui_marshal_VOID__STRING_SLIST_SLIST (GClosure     *closure,
					 GValue       *return_value G_GNUC_UNUSED,
					 guint         n_param_values,
					 const GValue *param_values,
					 gpointer      invocation_hint G_GNUC_UNUSED,
					 gpointer      marshal_data)
{
	typedef void (*GMarshalFunc_VOID__STRING_SLIST_SLIST) (gpointer     data1,
							       gpointer     arg_1,
							       gpointer     arg_2,
							       gpointer     arg_3,
							       gpointer     data2);
	register GMarshalFunc_VOID__STRING_SLIST_SLIST callback;
	register GCClosure *cc = (GCClosure*) closure;
	register gpointer data1, data2; 
	
	g_return_if_fail (n_param_values == 4);
	
	if (G_CCLOSURE_SWAP_DATA (closure)) {
		data1 = closure->data;
		data2 = g_value_peek_pointer (param_values + 0);
	}
	else {
		data1 = g_value_peek_pointer (param_values + 0);
		data2 = closure->data;
	}
	callback = (GMarshalFunc_VOID__STRING_SLIST_SLIST) (marshal_data ? marshal_data : cc->callback);
	
	callback (data1,
		  (gpointer) g_value_get_string (param_values + 1),
		  g_value_get_pointer (param_values + 2),
		  g_value_get_pointer (param_values + 3),
		  data2);
}

