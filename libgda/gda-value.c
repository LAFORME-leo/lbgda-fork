/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 Holger Thon <holger.thon@gnome-db.org>
 * Copyright (C) 2002 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2002 Zbigniew Chyla <cyba@gnome.pl>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dschoene@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@src.gnome.org>
 * Copyright (C) 2003 Philippe CHARLIER <p.charlier@chello.be>
 * Copyright (C) 2004 Andrew Hill <andru@src.gnome.org>
 * Copyright (C) 2004 Dani Baeyens <daniel.baeyens@hispalinux.es>
 * Copyright (C) 2004 - 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2004 Szalai Ferenc <szferi@einstein.ki.iif.hu>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Leonardo Boshell <lb@kmc.com.co>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libgda/gda-value.h>
#include <libgda/gda-blob-op.h>
#include <libgda/gda-util.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#define __GDA_INTERNAL__
#include "dir-blob-op.h"

#define l_g_value_unset(val) G_STMT_START{ if (G_IS_VALUE (val)) g_value_unset (val); }G_STMT_END
#ifdef G_OS_WIN32
#define bcmp(s1, s2, n) memcmp ((s1), (s2), (n))
#endif

static gboolean
set_from_string (GValue *value, const gchar *as_string)
{
	gboolean retval;
	gchar *endptr [1];
	gdouble dvalue;
	glong lvalue;
        gulong ulvalue;
	GType type;

	g_return_val_if_fail (value, FALSE);
	if (! G_IS_VALUE (value)) {
		g_warning ("Can't determine the GType of a NULL GValue");
		return FALSE;
	}

	type = G_VALUE_TYPE (value);
	g_value_reset (value);

	/* custom transform function */
	retval = FALSE;
	if (type == G_TYPE_BOOLEAN) {
		if ((as_string[0] == 't') || (as_string[0] == 'T')) {
			g_value_set_boolean (value, TRUE);
			retval = TRUE;
		}
		else if ((as_string[0] == 'f') || (as_string[0] == 'F')) {
			g_value_set_boolean (value, FALSE);
			retval = TRUE;
		}
		else {
			gint i;
			i = atoi (as_string); /* Flawfinder: ignore */
			g_value_set_boolean (value, i ? TRUE : FALSE);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_INT64) {
		gint64 i64;
		i64 = g_ascii_strtoll (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_int64 (value, i64);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UINT64) {
		guint64 ui64;
		ui64 = g_ascii_strtoull (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_uint64 (value, ui64);
			retval = TRUE;
                }
	}
	else if (type == G_TYPE_INT) {
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_int (value, (gint32) lvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UINT) {
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_uint(value,(guint32)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_SHORT) {
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string != '\0' && **endptr == '\0'){
			gda_value_set_short (value, (gint16) lvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_USHORT) {
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			gda_value_set_ushort(value,(guint16)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_CHAR) {
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_char(value,(gchar)lvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_UCHAR) {
		ulvalue = strtoul (as_string,endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_uchar(value,(guchar)ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_FLOAT) {
		dvalue = g_ascii_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_float (value, (gfloat) dvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_DOUBLE) {
		dvalue = g_ascii_strtod (as_string, endptr);
		if (*as_string != '\0' && **endptr == '\0'){
			g_value_set_double (value, dvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_NUMERIC) {
		GdaNumeric numeric;
		/* FIXME: what test whould i do for numeric? Use GMP (http://gmplib.org/) ?*/
		numeric.number = g_strdup (as_string);
		numeric.precision = 0; /* FIXME */
		numeric.width = 0; /* FIXME */
		gda_value_set_numeric (value, &numeric);
		g_free (numeric.number);
		retval = TRUE;
	}
	else if (type == G_TYPE_DATE) {
		GDate *gdate;
		gdate = g_date_new ();

		if (gda_parse_iso8601_date (gdate, as_string)) {
			g_value_take_boxed (value, gdate);
			retval = TRUE;
		}
		else
			g_date_free (gdate);
	}
	else if (type == GDA_TYPE_TIME) {
		GdaTime timegda;
		if (gda_parse_iso8601_time (&timegda, as_string)) {
			gda_value_set_time (value, &timegda);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_TIMESTAMP) {
		GdaTimestamp timestamp;
		if (gda_parse_iso8601_timestamp (&timestamp, as_string)) {
			gda_value_set_timestamp (value, &timestamp);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_NULL) {
		gda_value_set_null (value);
		retval = TRUE;
	}
	else if (type == G_TYPE_GTYPE) {
		if (gda_g_type_from_string (as_string) != 0) {
			g_value_set_gtype (value, gda_g_type_from_string (as_string));
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_ULONG)
	{
		gulong ulvalue;
		
		ulvalue = strtoul (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_ulong (value, ulvalue);
			retval = TRUE;
		}
	}
	else if (type == G_TYPE_LONG)
	{
		glong lvalue;
		
		lvalue = strtol (as_string, endptr, 10);
		if (*as_string!=0 && **endptr==0) {
			g_value_set_long (value, lvalue);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		bin = gda_string_to_binary (as_string);
		if (bin) {
			gda_value_take_binary (value, bin);
			retval = TRUE;
		}
	}
	else if (type == GDA_TYPE_BLOB) {
		GdaBlob *blob;
		blob = gda_string_to_blob (as_string);
		if (blob) {
			gda_value_take_blob (value, blob);
			retval = TRUE;
		}
	}


	if (!retval && g_value_type_transformable (G_TYPE_STRING, type)) {
		/* use the GLib type transformation function */
		GValue *string;

                string = g_new0 (GValue, 1);
                g_value_init (string, G_TYPE_STRING);
                g_value_set_string (string, as_string);

                g_value_transform (string, value);
                gda_value_free (string);

                retval = TRUE;
	}

	return retval;
}

/*
 * Register the DEFAULT type in the GType system
 */
static void
string_to_default (const GValue *src, GValue *dest)
{
       g_return_if_fail (G_VALUE_HOLDS_STRING (src) && GDA_VALUE_HOLDS_DEFAULT (dest));
       g_value_set_boxed (dest, g_value_get_string (src));
}

static void
default_to_string (const GValue *src, GValue *dest)
{
	gchar *str;
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) && GDA_VALUE_HOLDS_DEFAULT (src));
	str = (gchar*) g_value_get_boxed (src);
	g_value_set_string (dest, str);
}

static gpointer
gda_default_copy (G_GNUC_UNUSED gpointer boxed)
{
	if (boxed)
		return (gpointer) g_strdup (boxed);
	else
		return (gpointer) NULL;
}

static void
gda_default_free (G_GNUC_UNUSED gpointer boxed)
{
	g_free (boxed);
}

GType
gda_default_get_type (void)
{
       static GType type = 0;

       if (G_UNLIKELY (type == 0)) {
               type = g_boxed_type_register_static ("GdaDefault",
                                                    (GBoxedCopyFunc) gda_default_copy,
                                                    (GBoxedFreeFunc) gda_default_free);

               g_value_register_transform_func (G_TYPE_STRING,
                                                type,
                                                string_to_default);

               g_value_register_transform_func (type,
                                                G_TYPE_STRING,
                                                default_to_string);
       }

       return type;
}


/*
 * Register the GdaBinary type in the GType system
 */

/* Transform a String GValue to a GdaBinary*/
static void 
string_to_binary (const GValue *src, GValue *dest) 
{
	/* FIXME: add more checks*/
	GdaBinary *bin;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BINARY (dest));
	
	as_string = g_value_get_string (src);
	
	bin = gda_string_to_binary (as_string);
	g_return_if_fail (bin);
	gda_value_take_binary (dest, bin);
}

static void 
binary_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BINARY (src));
	
	str = gda_binary_to_string (gda_value_get_binary ((GValue *) src), 0);
	
	g_value_take_string (dest, str);
}

GType
gda_binary_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaBinary",
						     (GBoxedCopyFunc) gda_binary_copy,
						     (GBoxedFreeFunc) gda_binary_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_binary);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 binary_to_string);
	}
	
	return type;
}

/**
 * gda_binary_copy:
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaBinary structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaBinary which contains a copy of information in @boxed.
 *
 * Free-function: gda_binary_free
 */
gpointer
gda_binary_copy (gpointer boxed)
{
	GdaBinary *src = (GdaBinary*) boxed;
	GdaBinary *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBinary, 1);
	copy->data = g_memdup (src->data, src->binary_length);
	copy->binary_length = src->binary_length;
	
	return copy;
}

/**
 * gda_binary_free:
 * @boxed: (transfer full): #GdaBinary to free.
 *
 * Deallocates all memory associated to the given #GdaBinary.
 */
void
gda_binary_free (gpointer boxed)
{
	GdaBinary *binary = (GdaBinary*) boxed;
	
	g_return_if_fail (binary);
		
	g_free (binary->data);
	g_free (binary);
}

/* 
 * Register the GdaBlob type in the GType system 
 */
/* Transform a String GValue to a GdaBlob*/
static void 
string_to_blob (const GValue *src, GValue *dest) 
{
	/* FIXME: add more checks*/
	GdaBlob *blob;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_BLOB (dest));
	
	as_string = g_value_get_string (src);
	
	blob = gda_string_to_blob (as_string);
	g_return_if_fail (blob);
	gda_value_take_blob (dest, blob);
}

static void 
blob_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_BLOB (src));
	
	str = gda_blob_to_string ((GdaBlob *) gda_value_get_blob ((GValue *) src), 0);
	
	g_value_take_string (dest, str);
}

GType
gda_blob_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaBlob",
						     (GBoxedCopyFunc) gda_blob_copy,
						     (GBoxedFreeFunc) gda_blob_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_blob);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 blob_to_string);
	}
	
	return type;
}

/**
 * gda_blob_copy:
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaBlob structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaBlob which contains a copy of information in @boxed.
 *
 * Free-function: gda_blob_free
 */
gpointer
gda_blob_copy (gpointer boxed)
{
	GdaBlob *src = (GdaBlob*) boxed;
	GdaBlob *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaBlob, 1);
	if (((GdaBinary *)src)->data) {
		((GdaBinary *)copy)->data = g_memdup (((GdaBinary *)src)->data, ((GdaBinary *)src)->binary_length);
		((GdaBinary *)copy)->binary_length = ((GdaBinary *)src)->binary_length;
	}
	gda_blob_set_op (copy, src->op);
	
	return copy;
}

/**
 * gda_blob_free:
 * @boxed: (transfer full): #GdaBlob to free.
 *
 * Deallocates all memory associated to the given #GdaBlob.
 */
void
gda_blob_free (gpointer boxed)
{
	GdaBlob *blob = (GdaBlob*) boxed;
	
	g_return_if_fail (blob);

	if (blob->op) {
		g_object_unref (blob->op);
		blob->op = NULL;
	}
	gda_binary_free ((GdaBinary *) blob);
}

/**
 * gda_blob_set_op:
 * @blob: a #GdaBlob value
 * @op: (allow-none): a #GdaBlobOp object, or %NULL
 *
 * correctly assigns @op to @blob
 */
void
gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op)
{
	if (blob->op) {
		g_object_unref (blob->op);
		blob->op = NULL;
	}
	if (op) {
		g_return_if_fail (GDA_IS_BLOB_OP (op));
		blob->op = g_object_ref (op);
	}
}

/* 
 * Register the GdaGeometricPoint type in the GType system 
 */
static void
geometric_point_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	GdaGeometricPoint *point;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_GEOMETRIC_POINT (src));
	
	point = (GdaGeometricPoint *) gda_value_get_geometric_point ((GValue *) src);
	
	str = g_strdup_printf ("(%.*g,%.*g)",
				  DBL_DIG,
				  point->x,
				  DBL_DIG,
				  point->y);
	
	g_value_take_string (dest, str);
}

/* Transform a String GValue to a GdaGeometricPoint from a string like "(3.2,5.6)" */
static void
string_to_geometricpoint (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaGeometricPoint *point;
	const gchar *as_string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_GEOMETRIC_POINT (dest));
	
	as_string = g_value_get_string (src);
	point = g_new0 (GdaGeometricPoint, 1);
	
	as_string++;
	point->x = atof (as_string);
	as_string = strchr (as_string, ',');
	as_string++;
	point->y = atof (as_string);

	gda_value_set_geometric_point (dest, point);
	g_free (point);
}

GType
gda_geometricpoint_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaGeometricPoint",
						     (GBoxedCopyFunc) gda_geometricpoint_copy,
						     (GBoxedFreeFunc) gda_geometricpoint_free);
		
		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_geometricpoint);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 geometric_point_to_string);
	}
	
	return type;
}

/**
 * gda_geometricpoint_copy:
 *
 * Returns: (transfer full):
 */
gpointer 
gda_geometricpoint_copy (gpointer boxed)
{
	GdaGeometricPoint *val = (GdaGeometricPoint*) boxed;
	GdaGeometricPoint *copy;
	
	g_return_val_if_fail( val, NULL);
		
	copy = g_new0 (GdaGeometricPoint, 1);
	copy->x = val->x;
	copy->y = val->y;

	return copy;
}

void
gda_geometricpoint_free (gpointer boxed)
{
	g_free (boxed);
}




/* 
 * Register the GdaValueList type in the GType system 
 */
static gpointer gda_value_list_copy (gpointer boxed);
static void gda_value_list_free (gpointer boxed);

static void 
list_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	const GdaValueList *list;
	GList *l;
	GString *gstr = NULL;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_LIST (src));
	
	list = gda_value_get_list ((GValue *) src);
	
	for (l = (GList *) list; l != NULL; l = l->next) {
		str = gda_value_stringify ((GValue *) l->data);
		if (!gstr) {
			gstr = g_string_new ("{ ");
			gstr = g_string_append (gstr, str);
		}
		else {
			gstr = g_string_append (gstr, ", ");
			gstr = g_string_append (gstr, str);
		}
		g_free (str);
	}

	if (gstr) {
		g_string_append (gstr, " }");
		str = gstr->str;
		g_string_free (gstr, FALSE);
	}
	else
		str = g_strdup ("");
	
	g_value_take_string (dest, str);
}

GType
gda_value_list_get_type(void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaValueList",
						     (GBoxedCopyFunc) gda_value_list_copy,
						     (GBoxedFreeFunc) gda_value_list_free);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 list_to_string);
		/* FIXME: No function to transform from string to a GdaValueList */
	}

	return type;
}

static gpointer 
gda_value_list_copy (gpointer boxed)
{
	GList *list = NULL;
	const GList *values;
	
	values = (GList*) boxed;
	
	while (values) {
		list = g_list_append (list, gda_value_copy ((GValue *) (values->data)));
		values = values->next;
	}

	return list;
}

static void
gda_value_list_free (gpointer boxed)
{
	GList *l = (GList*) boxed;
	g_list_free (l);
}





/* 
 * Register the GdaNumeric type in the GType system 
 */
static void 
numeric_to_string (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_string (dest, numeric->number);
	else
		g_value_set_string (dest, "");
}

static void 
numeric_to_int (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_INT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric) {
		glong tmp;
		tmp = atol (numeric->number); /* Flawfinder: ignore */
		if ((tmp < G_MININT) || (tmp > G_MAXINT))
			g_warning ("Integer overflow for value %ld", tmp);
		g_value_set_int (dest, tmp);
	}
	else
		g_value_set_int (dest, 0);
}

static void 
numeric_to_uint (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_UINT (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric) {
		glong tmp;
		tmp = atol (numeric->number); /* Flawfinder: ignore */
		if ((tmp < 0) || (tmp > G_MAXUINT))
			g_warning ("Unsigned integer overflow for value %ld", tmp);
		g_value_set_uint (dest, tmp);
	}
	else
		g_value_set_uint (dest, 0);
}

static void 
numeric_to_boolean (const GValue *src, GValue *dest) 
{
	const GdaNumeric *numeric;
	
	g_return_if_fail (G_VALUE_HOLDS_BOOLEAN (dest) &&
			  GDA_VALUE_HOLDS_NUMERIC (src));

	numeric = gda_value_get_numeric (src);
	if (numeric)
		g_value_set_boolean (dest, atoi (numeric->number)); /* Flawfinder: ignore */
	else
		g_value_set_boolean (dest, 0);
}

GType
gda_numeric_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaNumeric",
						     (GBoxedCopyFunc) gda_numeric_copy,
						     (GBoxedFreeFunc) gda_numeric_free);
	
		/* FIXME: No function to Transform String to from GdaNumeric */
	
		g_value_register_transform_func (type, G_TYPE_STRING, numeric_to_string);
		g_value_register_transform_func (type, G_TYPE_INT, numeric_to_int);
		g_value_register_transform_func (type, G_TYPE_UINT, numeric_to_uint);
		g_value_register_transform_func (type, G_TYPE_BOOLEAN, numeric_to_boolean);
	}
	
	return type;
}

/**
 * gda_numeric_copy:
 * @boxed: source to get a copy from.
 *
 * Creates a new #GdaNumeric structure from an existing one.

 * Returns: (transfer full): a newly allocated #GdaNumeric which contains a copy of information in @boxed.
 *
 * Free-function: gda_numeric_free
 */

gpointer
gda_numeric_copy (gpointer boxed)
{
	GdaNumeric *src = (GdaNumeric*) boxed;
	GdaNumeric *copy = NULL;

	g_return_val_if_fail (src, NULL);

	copy = g_new0 (GdaNumeric, 1);
	copy->number = g_strdup (src->number);
	copy->precision = src->precision;
	copy->width = src->width;  

	return copy;
}

/**
 * gda_numeric_free:
 * @boxed: (transfer full): a #GdaNumeric pointer
 *
 * Deallocates all memory associated to the given @boxed
 */
void
gda_numeric_free (gpointer boxed)
{
	GdaNumeric *numeric = (GdaNumeric*) boxed;
	g_return_if_fail (numeric);

	g_free (numeric->number);
	g_free (numeric);
}



/* 
 * Register the GdaTime type in the GType system 
 */

static void 
time_to_string (const GValue *src, GValue *dest) 
{
	GdaTime *gdatime;
	GString *string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_TIME (src));
	
	gdatime = (GdaTime *) gda_value_get_time ((GValue *) src);
	
	string = g_string_new ("");
	g_string_append_printf (string, "%02u:%02u:%02u",
				gdatime->hour,
				gdatime->minute,
				gdatime->second);
	if (gdatime->fraction != 0)
		g_string_append_printf (string, ".%lu", gdatime->fraction);
	
	if (gdatime->timezone != GDA_TIMEZONE_INVALID)
		g_string_append_printf (string, "%+02d", (int) gdatime->timezone / 3600);

	g_value_take_string (dest, string->str);
	g_string_free (string, FALSE);
}

/* Transform a String GValue to a GdaTime from a string like "12:30:15+01" */
static void
string_to_time (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaTime *timegda;
	const gchar *as_string;
	const gchar *ptr;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_TIME (dest));
       
	as_string = g_value_get_string (src);
	if (!as_string)
		return;
	timegda = g_new0 (GdaTime, 1);

	/* hour */
	timegda->timezone = GDA_TIMEZONE_INVALID;
	ptr = as_string;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->hour = (*ptr - '0') * 10 + *(ptr+1) - '0';
	else {
		g_free (timegda);
		return;
	}

	/* minute */
	ptr += 2;
	if (! *ptr) {
		g_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->minute = (*ptr - '0') * 10 + *(ptr+1) - '0';
	else{
		g_free (timegda);
		return;
	}

	/* second */
	ptr += 2;
	timegda->second = 0;
	if (! *ptr) {
		if ((timegda->hour <= 24) && (timegda->minute <= 60))
			gda_value_set_time (dest, timegda);
		g_free (timegda);
		return;
	}
	if (*ptr == ':')
		ptr++;
	if ((*ptr >= '0') && (*ptr <= '9') &&
	    (*(ptr+1) >= '0') && (*(ptr+1) <= '9'))
		timegda->second = (*ptr - '0') * 10 + *(ptr+1) - '0';
	
	/* extra */
	ptr += 2;
	if (! *ptr) {
		if ((timegda->hour <= 24) && (timegda->minute <= 60) && 
		    (timegda->second <= 60))
			gda_value_set_time (dest, timegda);
		g_free (timegda);
		return;
	}

	if (*ptr == '.') {
		gulong fraction = 0;

		ptr ++;
		while (*ptr && (*ptr >= '0') && (*ptr <= '9')) {
			fraction = fraction * 10 + *ptr - '0';
			ptr++;
		}
	}

	if ((*ptr == '+') || (*ptr == '-')) {
		glong sign = (*ptr == '+') ? 1 : -1;
		timegda->timezone = 0;
		while (*ptr && (*ptr >= '0') && (*ptr <= '9')) {
			timegda->timezone = timegda->timezone * 10 + sign * ((*ptr) - '0');
			ptr++;
		}
		timegda->timezone *= 3600;
	}
	
	/* checks */
	if ((timegda->hour <= 24) || (timegda->minute <= 60) || (timegda->second <= 60))
		gda_value_set_time (dest, timegda);
	g_free (timegda);
}

GType
gda_time_get_type(void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaTime",
						     (GBoxedCopyFunc) gda_time_copy,
						     (GBoxedFreeFunc) gda_time_free);
	
		g_value_register_transform_func (G_TYPE_STRING, type, string_to_time);
		g_value_register_transform_func (type, G_TYPE_STRING, time_to_string);
	}
	
	return type;
}

/**
 * gda_time_copy:
 *
 * Returns: (transfer full):
 */
gpointer
gda_time_copy (gpointer boxed)
{
	
	GdaTime *src = (GdaTime*) boxed;
	GdaTime *copy = NULL;
	
	g_return_val_if_fail (src, NULL);
	
	copy = g_new0 (GdaTime, 1);
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;

	return copy;
}

void
gda_time_free (gpointer boxed)
{
	g_free (boxed);
}

/**
 * gda_time_valid:
 * @time: a #GdaTime value to check if it is valid
 *
 * Returns: #TRUE if #GdaTime is valid; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
gda_time_valid (const GdaTime *time)
{
	g_return_val_if_fail (time, FALSE);

	if (time->hour > 24 ||
	    time->minute > 59 ||
	    time->second > 59)
		return FALSE;
	else
		return TRUE;
}


/* 
 * Register the GdaTimestamp type in the GType system 
 */
/* Transform a String GValue to a GdaTimestamp from a string like "2003-12-13 13:12:01.12+01" */
static void
string_to_timestamp (const GValue *src, GValue *dest)
{
	/* FIXME: add more checks*/
	GdaTimestamp timestamp;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  GDA_VALUE_HOLDS_TIMESTAMP (dest));

	if (! gda_parse_iso8601_timestamp (&timestamp, g_value_get_string (src)))
		g_warning ("Can't convert '%s' to a timestamp", g_value_get_string (src));
	gda_value_set_timestamp (dest, &timestamp);
}

static void 
timestamp_to_string (const GValue *src, GValue *dest) 
{
	GdaTimestamp *timestamp;
	GString *string;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  GDA_VALUE_HOLDS_TIMESTAMP (src));
	
	timestamp = (GdaTimestamp *) gda_value_get_timestamp ((GValue *) src);
	
	string = g_string_new ("");
	g_string_append_printf (string, "%04u-%02u-%02u %02u:%02u:%02u",
				timestamp->year,
				timestamp->month,
				timestamp->day,
				timestamp->hour,
				timestamp->minute,
				timestamp->second);
	if (timestamp->fraction > 0)
		g_string_append_printf (string, ".%lu", timestamp->fraction);
	if (timestamp->timezone != GDA_TIMEZONE_INVALID)
		g_string_append_printf (string, "%+02d",
					(int) timestamp->timezone/3600);
	g_value_take_string (dest, string->str);
	g_string_free (string, FALSE);
}

GType
gda_timestamp_get_type (void)
{
	static GType type = 0;
	
	if (G_UNLIKELY (type == 0)) {
		type = g_boxed_type_register_static ("GdaTimestamp",
						     (GBoxedCopyFunc) gda_timestamp_copy,
						     (GBoxedFreeFunc) gda_timestamp_free);
	
		g_value_register_transform_func (G_TYPE_STRING, type, string_to_timestamp);
		g_value_register_transform_func (type, G_TYPE_STRING, timestamp_to_string);
	}
	
	return type;
}

/**
 * gda_timestamp_copy:
 *
 * Returns: (transfer full):
 */
gpointer 
gda_timestamp_copy (gpointer boxed)
{
	GdaTimestamp *src = (GdaTimestamp*) boxed;
	GdaTimestamp *copy;
	
	g_return_val_if_fail(src, NULL);
	
	copy = g_new0 (GdaTimestamp, 1);
	copy->year = src->year;
	copy->month = src->month;
	copy->day = src->day;
	copy->hour = src->hour;
	copy->minute = src->minute;
	copy->second = src->second;
	copy->fraction = src->fraction;
	copy->timezone = src->timezone;
	
	return copy;
}

void
gda_timestamp_free (gpointer boxed)
{
	g_free (boxed);
}

/**
 * gda_timestamp_valid:
 * @timestamp: a #GdaTimestamp value to check if it is valid
 *
 * Returns: #TRUE if #GdaTimestamp is valid; %FALSE otherwise.
 *
 * Since: 4.2
 */
gboolean
gda_timestamp_valid (const GdaTimestamp *timestamp)
{
	g_return_val_if_fail (timestamp, FALSE);

	GDate *gdate;

	/* check the date part */
	gdate = g_date_new_dmy ((GDateDay) timestamp->day, (GDateMonth) timestamp->month,
				(GDateYear) timestamp->year);
	if (!gdate)
		return FALSE;
	if (! g_date_valid (gdate)) {
		g_date_free (gdate);
		return FALSE;
	}
	g_date_free (gdate);

	/* check the time part */
	if (timestamp->hour > 24 ||
	    timestamp->minute > 59 ||
	    timestamp->second > 59)
		return FALSE;
	else
		return TRUE;
}

/**
 * gda_value_new:
 * @type: the new value type.
 *
 * Makes a new #GValue of type @type.
 *
 * Returns: (transfer full): the newly created #GValue with the specified @type. You need to set the value in the returned GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new (GType type)
{
	GValue *value;

	value = g_new0 (GValue, 1);
	g_value_init (value, type);

	return value;
}

/**
 * gda_value_new_default:
 * @default_val: (allow-none): the default value as a string, or %NULL
 *
 * Creates a new default value.
 *
 * Returns: (transfer full): a new #GValue of the type #GDA_TYPE_DEFAULT
 *
 * Since: 4.2.9
 */
GValue *
gda_value_new_default (const gchar *default_val)
{
	GValue *value;
	value = gda_value_new (GDA_TYPE_DEFAULT);
	g_value_set_boxed (value, default_val);
	return value;
}

/**
 * gda_value_new_binary:
 * @val: value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BINARY with value @val.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_binary (const guchar *val, glong size)
{
	GValue *value;
	GdaBinary binary;

	/* We use the const on the function parameter to make this clearer, 
	 * but it would be awkward to keep the const in the struct.
         */
        binary.data = (guchar*)val;
        binary.binary_length = size;

        value = g_new0 (GValue, 1);
        gda_value_set_binary (value, &binary);

        return value;
}

/**
 * gda_value_new_blob:
 * @val: value to set for the new #GValue.
 * @size: the size of the memory pool pointer to by @val.
 *
 * Makes a new #GValue of type #GDA_TYPE_BLOB with the data contained by @val.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_blob (const guchar *val, glong size)
{
	GValue *value;
	GdaBlob *blob;
	GdaBinary *bin;

	blob = g_new0 (GdaBlob, 1);
	bin = (GdaBinary*)(blob);
	bin->data = g_new (guchar, size);
        memcpy ((gpointer) bin->data, (gpointer) val, size); /* Flawfinder: ignore */
        bin->binary_length = size;
	blob->op = NULL;

        value = g_new0 (GValue, 1);
	g_value_init (value, GDA_TYPE_BLOB);
        g_value_take_boxed (value, blob);

        return value;
}

/**
 * gda_value_new_blob_from_file:
 * @filename: name of the file to manipulate
 *
 * Makes a new #GValue of type #GDA_TYPE_BLOB interfacing with the contents of the file
 * named @filename
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_blob_from_file (const gchar *filename)
{
	GValue *value;
	GdaBlob *blob;

	blob = g_new0 (GdaBlob, 1);
	blob->op = gda_dir_blob_op_new (filename);

        value = g_new0 (GValue, 1);
	g_value_init (value, GDA_TYPE_BLOB);
        g_value_take_boxed (value, blob);

        return value;
}

/**
 * gda_value_new_timestamp_from_timet:
 * @val: value to set for the new #GValue.
 *
 * Makes a new #GValue of type #GDA_TYPE_TIMESTAMP with value @val
 * (of type time_t).
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_timestamp_from_timet (time_t val)
{
	GValue *value;

	struct tm *ltm;

        value = g_new0 (GValue, 1);
#ifdef HAVE_LOCALTIME_R
	struct tm tmpstm;
	ltm = localtime_r ((const time_t *) &val, &tmpstm);
#elif HAVE_LOCALTIME_S
	struct tm tmpstm;
	if (localtime_s (&tmpstm, (const time_t *) &val) == 0)
		ltm = &tmpstm;
	else
		ltm = NULL;
#else
	ltm = localtime ((const time_t *) &val);
#endif

        if (ltm) {
                GdaTimestamp tstamp;
                tstamp.year = ltm->tm_year + 1900;
                tstamp.month = ltm->tm_mon + 1;
                tstamp.day = ltm->tm_mday;
                tstamp.hour = ltm->tm_hour;
                tstamp.minute = ltm->tm_min;
                tstamp.second = ltm->tm_sec;
                tstamp.fraction = 0;
                tstamp.timezone = GDA_TIMEZONE_INVALID;
                gda_value_set_timestamp (value, (const GdaTimestamp *) &tstamp);
        }

        return value;
}

/**
 * gda_value_new_from_string:
 * @as_string: stringified representation of the value.
 * @type: the new value type.
 *
 * Makes a new #GValue of type @type from its string representation. 
 *
 * For more information
 * about the string format, see the gda_value_set_from_string() function.
 * This function is typically used when reading configuration files or other non-user input that should be locale 
 * independent.
 *
 * Returns: (transfer full): the newly created #GValue or %NULL if the string representation cannot be converted to the specified @type.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_from_string (const gchar *as_string, GType type)
{
	GValue *value;
	
	g_return_val_if_fail (as_string, NULL);
	
	value = gda_value_new (type);
	if (set_from_string (value, as_string))
		return value;
	else {
		gda_value_free (value);
		return NULL;
	}
}

/**
 * gda_value_new_from_xml:
 * @node: an XML node representing the value.
 *
 * Creates a GValue from an XML representation of it. That XML
 * node corresponds to the following string representation:
 * &lt;value type="gdatype"&gt;value&lt;/value&gt;
 *
 * For more information
 * about the string format, see the gda_value_set_from_string() function.
 * This function is typically used when reading configuration files or other non-user input that should be locale 
 * independent.
 *
 * Returns: (transfer full): the newly created #GValue.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_new_from_xml (const xmlNodePtr node)
{
	GValue *value;
	xmlChar *prop;

	g_return_val_if_fail (node, NULL);

	/* parse the XML */
	if (!node || !(node->name) || (node && strcmp ((gchar*)node->name, "value"))) 
		return NULL;

	value = g_new0 (GValue, 1);
	prop = xmlGetProp (node, (xmlChar*)"gdatype");
	if (prop && !gda_value_set_from_string (value,
						(gchar*)xmlNodeGetContent (node),
						gda_g_type_from_string ((gchar*) prop))) {
		g_free (value);
		value = NULL;
	}
	if (prop)
		xmlFree (prop);

	return value;
}

/**
 * gda_value_free:
 * @value: (transfer full) (allow-none): the resource to free (or %NULL)
 *
 * Deallocates all memory associated to a #GValue.
 */
void
gda_value_free (GValue *value)
{
	if (!value)
		return;

	l_g_value_unset (value);
	g_free (value);
}

/* gda_value_reset_with_type
 * @value: the #GValue to be reseted
 * @type:  the #GType to set to
 *
 * Resets the #GValue and set a new type to #GType.
*/
void
gda_value_reset_with_type (GValue *value, GType type)
{
	g_return_if_fail (value);

	if (G_IS_VALUE (value) && (G_VALUE_TYPE (value) == type))
		g_value_reset (value);
	else {
		l_g_value_unset (value);
		if (type == GDA_TYPE_NULL || type == G_TYPE_INVALID)
			return;
		else
			g_value_init (value, type);
	}
}



/**
 * gda_value_is_null:
 * @value: value to test.
 *
 * Tests if a given @value is of type #GDA_TYPE_NULL.
 *
 * Returns: a boolean that says whether or not @value is of type #GDA_TYPE_NULL.
 */
gboolean
gda_value_is_null (const GValue *value)
{
	g_return_val_if_fail (value, FALSE);
	return !G_IS_VALUE (value);
}

/**
 * gda_value_is_number:
 * @value: a #GValue.
 *
 * Gets whether the value stored in the given #GValue is of numeric type or not.
 *
 * Returns: %TRUE if a number, %FALSE otherwise.
 */
gboolean
gda_value_is_number (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE(value), FALSE);
	if(G_VALUE_HOLDS_INT(value) ||
		G_VALUE_HOLDS_INT64(value) ||
		G_VALUE_HOLDS_UINT(value) ||
		G_VALUE_HOLDS_UINT64(value) ||
		G_VALUE_HOLDS_CHAR(value) ||
		G_VALUE_HOLDS_UCHAR(value))
		return TRUE;
	else
		return FALSE;
}

/**
 * gda_value_copy:
 * @value: value to get a copy from.
 *
 * Creates a new #GValue from an existing one.
 * 
 * Returns: (transfer full): a newly allocated #GValue with a copy of the data in @value.
 *
 * Free-function: gda_value_free
 */
GValue *
gda_value_copy (const GValue *value)
{
	GValue *copy;
	
	g_return_val_if_fail (value, NULL);

	copy = g_new0 (GValue, 1);

	if (G_IS_VALUE (value)) {
		g_value_init (copy, G_VALUE_TYPE (value));
		g_value_copy (value, copy);
	}

	return copy;
}

/**
 * gda_value_get_binary:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaBinary *
gda_value_get_binary (const GValue *value)
{
	GdaBinary *val;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BINARY), NULL);

	val = (GdaBinary*) g_value_get_boxed (value);

	return val;
}


/**
 * gda_value_set_binary:
 * @value: a #GValue that will store @val.
 * @binary: a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_binary (GValue *value, const GdaBinary *binary)
{
	g_return_if_fail (value);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	if (binary)
		g_value_set_boxed (value, binary);
	else {
		GdaBinary bin = {NULL, 0};
		g_value_set_boxed (value, &bin);
	}
}

/**
 * gda_value_take_binary:
 * @value: a #GValue that will store @val.
 * @binary: (transfer full): a #GdaBinary structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_binary(), the @binary
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_binary (GValue *value, GdaBinary *binary)
{
	g_return_if_fail (value);
	g_return_if_fail (binary);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BINARY);
	g_value_take_boxed (value, binary);
}

/**
 * gda_value_set_blob:
 * @value: a #GValue that will store @val.
 * @blob: a #GdaBlob structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_blob (GValue *value, const GdaBlob *blob)
{
	g_return_if_fail (value);
	g_return_if_fail (blob);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_set_boxed (value, blob);
}

/**
 * gda_value_get_blob:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaBlob *
gda_value_get_blob (const GValue *value)
{
	GdaBlob *val;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_BLOB), NULL);

	val = (GdaBlob*) g_value_get_boxed (value);

	return val;
}

/**
 * gda_value_take_blob:
 * @value: a #GValue that will store @val.
 * @blob: (transfer full): a #GdaBlob structure with the data and its size to be stored in @value.
 *
 * Stores @val into @value, but on the contrary to gda_value_set_blob(), the @blob
 * argument is not copied, but used as-is and it should be considered owned by @value.
 */
void
gda_value_take_blob (GValue *value, GdaBlob *blob)
{
	g_return_if_fail (value);
	g_return_if_fail (blob);
	
	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_BLOB);
	g_value_take_boxed (value, blob);
}

/**
 * gda_value_get_geometric_point:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaGeometricPoint *
gda_value_get_geometric_point (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_GEOMETRIC_POINT), NULL);
	return (const GdaGeometricPoint *) g_value_get_boxed(value);
}

/**
 * gda_value_set_geometric_point:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_geometric_point (GValue *value, const GdaGeometricPoint *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);	
	g_value_init (value, GDA_TYPE_GEOMETRIC_POINT);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_list:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaValueList *
gda_value_get_list (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_LIST), NULL);
	return (const GdaValueList *) g_value_get_boxed(value);
}

/**
 * gda_value_set_list:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_list (GValue *value, const GdaValueList *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_LIST);
	
	/* See the implementation of GdaValueList as a GBoxed for the Copy function used by GValue*/
	g_value_set_boxed (value, val);
}

/**
 * gda_value_set_null:
 * @value: a #GValue that will store a value of type #GDA_TYPE_NULL.
 *
 * Sets the type of @value to #GDA_TYPE_NULL.
 */
void
gda_value_set_null (GValue *value)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
}

/**
 * gda_value_get_numeric:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaNumeric *
gda_value_get_numeric (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_NUMERIC), NULL);
	return (const GdaNumeric *) g_value_get_boxed(value);
}

/**
 * gda_value_set_numeric:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_numeric (GValue *value, const GdaNumeric *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_NUMERIC);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_short:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gshort
gda_value_get_short (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_SHORT), -1);
	return (gshort) value->data[0].v_int;
}

/**
 * gda_value_set_short:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_short (GValue *value, gshort val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_SHORT);
	value->data[0].v_int = val;
}

/**
 * gda_value_get_ushort:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: the value stored in @value.
 */
gushort
gda_value_get_ushort (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), -1);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_USHORT), -1);
	return (gushort) value->data[0].v_uint;
}

/**
 * gda_value_set_ushort:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_ushort (GValue *value, gushort val)
{
	g_return_if_fail (value);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_USHORT);
	value->data[0].v_uint = val;
}


/**
 * gda_value_get_time:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaTime *
gda_value_get_time (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIME), NULL);
	return (const GdaTime *) g_value_get_boxed(value);
}

/**
 * gda_value_set_time:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_time (GValue *value, const GdaTime *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_TIME);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_get_timestamp:
 * @value: a #GValue whose value we want to get.
 *
 * Returns: (transfer none): the value stored in @value.
 */
const GdaTimestamp *
gda_value_get_timestamp (const GValue *value)
{
	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);
	g_return_val_if_fail (gda_value_isa (value, GDA_TYPE_TIMESTAMP), NULL);
	return (const GdaTimestamp *) g_value_get_boxed(value);
}

/**
 * gda_value_set_timestamp:
 * @value: a #GValue that will store @val.
 * @val: value to be stored in @value.
 *
 * Stores @val into @value.
 */
void
gda_value_set_timestamp (GValue *value, const GdaTimestamp *val)
{
	g_return_if_fail (value);
	g_return_if_fail (val);

	l_g_value_unset (value);
	g_value_init (value, GDA_TYPE_TIMESTAMP);
	g_value_set_boxed (value, val);
}

/**
 * gda_value_set_from_string:
 * @value: a #GValue that will store @val.
 * @as_string: the stringified representation of the value.
 * @type: the type of the value
 *
 * Stores the value data from its string representation as @type.
 *
 * The accepted formats are:
 * <itemizedlist>
 *   <listitem><para>G_TYPE_BOOLEAN: a caseless comparison is made with "true" or "false"</para></listitem>
 *   <listitem><para>numerical types: C locale format (dot as a fraction separator)</para></listitem>
 *   <listitem><para>G_TYPE_DATE: see <link linkend="gda-parse-iso8601-date">gda_parse_iso8601_date()</link></para></listitem>
 *   <listitem><para>GDA_TYPE_TIME: see <link linkend="gda-parse-iso8601-time">gda_parse_iso8601_time()</link></para></listitem>
 *   <listitem><para>GDA_TYPE_TIMESTAMP: see <link linkend="gda-parse-iso8601-timestamp">gda_parse_iso8601_timestamp()</link></para></listitem>
 * </itemizedlist>
 *
 * This function is typically used when reading configuration files or other non-user input that should be locale 
 * independent.
 *
 * Returns: %TRUE if the value has been converted to @type from
 * its string representation; it not means that the value is converted 
 * successfully, just that the transformation is available. %FALSE otherwise.
 */
gboolean
gda_value_set_from_string (GValue *value, 
			   const gchar *as_string,
			   GType type)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (as_string, FALSE);
		
	/* REM: glib does not register any transform function from G_TYPE_STRING to any other
	 * type except to a G_TYPE_STRING, so we can't use g_value_type_transformable (G_TYPE_STRING, type) */
	gda_value_reset_with_type (value, type);
        return set_from_string (value, as_string);
}

/**
 * gda_value_set_from_value:
 * @value: a #GValue.
 * @from: the value to copy from.
 *
 * Sets the value of a #GValue from another #GValue. This
 * is different from #gda_value_copy, which creates a new #GValue.
 * #gda_value_set_from_value, on the other hand, copies the contents
 * of @copy into @value, which must already be allocated.
 *
 * Returns: %TRUE if successful, %FALSE otherwise.
 */
gboolean
gda_value_set_from_value (GValue *value, const GValue *from)
{
	g_return_val_if_fail (value, FALSE);
	g_return_val_if_fail (from, FALSE);

	if (G_IS_VALUE (from)) {
		if (g_value_type_compatible (G_VALUE_TYPE (from), G_VALUE_TYPE (value))) {
			g_value_reset (value);
			g_value_copy (from, value);
			return TRUE;
		}
		else
			return FALSE;
	}
	else {
		l_g_value_unset (value);
		return TRUE;
	}
}

/**
 * gda_value_stringify:
 * @value: a #GValue.
 *
 * Converts a GValue to its string representation which is a human readable value. Note that the
 * returned string does not take into account the current locale of the user (on the contrary to the
 * #GdaDataHandler objects). Using this function should be limited to debugging and values serialization
 * purposes.
 *
 * Dates are converted in a YYYY-MM-DD format.
 *
 * Returns: (transfer full): a new string, or %NULL if the conversion cannot be done. Free the value with a g_free() when you've finished using it. 
 */
gchar *
gda_value_stringify (const GValue *value)
{
	if (value && G_IS_VALUE (value)) {
		if (g_value_type_transformable (G_VALUE_TYPE (value), G_TYPE_STRING)) {
			GValue *string;
			gchar *str;
			
			string = g_value_init (g_new0 (GValue, 1), G_TYPE_STRING);
			g_value_transform (value, string);
			str = g_value_dup_string (string);
			gda_value_free (string);
			return str;
		}
		else {
			GType type = G_VALUE_TYPE (value);
			if (type == G_TYPE_DATE) {
				GDate *date;
				date = (GDate *) g_value_get_boxed (value);
				if (date) {
					if (g_date_valid (date))
						return g_strdup_printf ("%04u-%02u-%02u",
									g_date_get_year (date),
									g_date_get_month (date),
									g_date_get_day (date));
					else
						return g_strdup_printf ("%04u-%02u-%02u",
									date->year, date->month, date->day);
				}
				else
					return g_strdup ("0000-00-00");
			}
			else if (type == GDA_TYPE_LIST) {
				const GdaValueList *list;
				const GList *ptr;
				GString *string;
				gchar *tmp;

				string = g_string_new ("[");
				list = gda_value_get_list (value);
				for (ptr = list; ptr; ptr = ptr->next) {
					tmp = gda_value_stringify ((GValue *) ptr->data);
					if (ptr != list)
						g_string_append_c (string, ',');
					g_string_append (string, tmp);
					g_free (tmp);
				}
				g_string_append_c (string, ']');
				tmp = string->str;
				g_string_free (string, FALSE);
				return tmp;
			}
			else
				return g_strdup ("");
		}
	}
	else
		return g_strdup ("NULL");
}
	
/**
 * gda_value_differ:
 * @value1: a #GValue to compare.
 * @value2: the other #GValue to be compared to @value1.
 *
 * Tells if two values are equal or not, by comparing memory representations. Unlike gda_value_compare(),
 * the returned value is boolean, and gives no idea about ordering.
 *
 * The two values must be of the same type, with the exception that a value of any type can be
 * compared to a GDA_TYPE_NULL value, specifically:
 * <itemizedlist>
 *   <listitem><para>if @value1 and @value2 are both GDA_TYPE_NULL values then the returned value is 0</para></listitem>
 *   <listitem><para>if @value1 is a GDA_TYPE_NULL value and @value2 is of another type then the returned value is 1</para></listitem>
 *   <listitem><para>if @value1 is of another type and @value2 is a GDA_TYPE_NULL value then the returned value is 1</para></listitem>
 *   <listitem><para>in all other cases, @value1 and @value2 must be of the same type and their values are compared</para></listitem>
 * </itemizedlist>
 *
 * Returns: a non 0 value if @value1 and @value2 differ, and 0 if they are equal
 */
gint
gda_value_differ (const GValue *value1, const GValue *value2)
{
	GType type;
	g_return_val_if_fail (value1 && value2, FALSE);

	/* blind value comparison */
	type = G_VALUE_TYPE (value1);
	if (!bcmp (value1, value2, sizeof (GValue)))
		return 0;

	/* handle GDA_TYPE_NULL comparisons with other types */
	else if (type == GDA_TYPE_NULL) {
		if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
			return 0;
		else
			return 1;
	}

	else if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
		return 1;

	g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), 1);

	/* general case */
	if (type == GDA_TYPE_BINARY) {
		const GdaBinary *binary1 = gda_value_get_binary (value1);
		const GdaBinary *binary2 = gda_value_get_binary (value2);
		if (binary1 && binary2 && (binary1->binary_length == binary2->binary_length))
			return bcmp (binary1->data, binary2->data, binary1->binary_length);
		else
			return 1;
	}

	else if (type == GDA_TYPE_BLOB) {
		const GdaBlob *blob1 = gda_value_get_blob (value1);
		const GdaBlob *blob2 = gda_value_get_blob (value2);
		if (blob1 && blob2 && (((GdaBinary *)blob1)->binary_length == ((GdaBinary *)blob2)->binary_length)) {
			if (blob1->op == blob2->op)
				return bcmp (((GdaBinary *)blob1)->data, ((GdaBinary *)blob2)->data, 
					     ((GdaBinary *)blob1)->binary_length);
		}
		return 1;
	}

	else if (type == G_TYPE_DATE) {
		GDate *d1, *d2;

		d1 = (GDate *) g_value_get_boxed (value1);
		d2 = (GDate *) g_value_get_boxed (value2);
		if (d1 && d2)
			return g_date_compare (d1, d2);
		return 1;
	}

	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		const GdaGeometricPoint *p1, *p2;
		p1 = gda_value_get_geometric_point (value1);
		p2 = gda_value_get_geometric_point (value2);
		if (p1 && p2)
			return bcmp (p1, p2, sizeof (GdaGeometricPoint));
		return 1;
	}

	else if (type == G_TYPE_OBJECT) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	else if (type == GDA_TYPE_LIST) {
		GList *l1, *l2;
		for (l1 = (GList*) gda_value_get_list (value1), l2 = (GList*) gda_value_get_list (value2); 
		     l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next){
			if (gda_value_differ ((GValue *) l1->data, (GValue *) l2->data))
				return 1;
		}
		return 0;
	}

	else if (type == GDA_TYPE_NUMERIC) {
		const GdaNumeric *num1, *num2;
		num1= gda_value_get_numeric (value1);
		num2 = gda_value_get_numeric (value2);
                if (num1 && num2)
			return strcmp (num1->number, num2->number);
		return 1;
	}
	
	else if (type == G_TYPE_STRING)	{
		const gchar *str1, *str2;
		str1 = g_value_get_string (value1);
		str2 = g_value_get_string (value2);
		if (str1 && str2)
			return strcmp (str1, str2);
		return 1;
	}
	
	else if (type == GDA_TYPE_TIME) {
		const GdaTime *t1, *t2;
		t1 = gda_value_get_time (value1);
		t2 = gda_value_get_time (value2);
		if (t1 && t2)
			return bcmp (t1, t2, sizeof (GdaTime));
		return 1;
	}

	else if (type == GDA_TYPE_TIMESTAMP) {
		const GdaTimestamp *ts1, *ts2;
		ts1 = gda_value_get_timestamp (value1);
		ts2 = gda_value_get_timestamp (value2);
		if (ts1 && ts2)
			return bcmp (ts1, ts2, sizeof (GdaTimestamp));
		return 1;
	}

	else if ((type == G_TYPE_INT) ||
		 (type == G_TYPE_UINT) ||
		 (type == G_TYPE_INT64) ||
		 (type == G_TYPE_UINT64) ||
		 (type == GDA_TYPE_SHORT) ||
		 (type == GDA_TYPE_USHORT) ||
		 (type == G_TYPE_FLOAT) ||
		 (type == G_TYPE_DOUBLE) ||
		 (type == G_TYPE_BOOLEAN) ||
		 (type == G_TYPE_CHAR) ||
		 (type == G_TYPE_UCHAR) ||
		 (type == G_TYPE_LONG) ||
		 (type == G_TYPE_ULONG) ||
		 (type == G_TYPE_GTYPE))
		/* values here ARE different because otherwise the bcmp() at the beginning would
		 * already have retruned */
		return 1;

	else if (g_type_is_a (type, G_TYPE_OBJECT)) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	g_warning ("%s() cannot handle values of type %s", __FUNCTION__, g_type_name (G_VALUE_TYPE (value1)));

	return 1;
}

/**
 * gda_value_compare:
 * @value1: a #GValue to compare (not %NULL)
 * @value2: the other #GValue to be compared to @value1 (not %NULL)
 *
 * Compares two values of the same type, with the exception that a value of any type can be
 * compared to a GDA_TYPE_NULL value, specifically:
 * <itemizedlist>
 *   <listitem><para>if @value1 and @value2 are both GDA_TYPE_NULL values then the returned value is 0</para></listitem>
 *   <listitem><para>if @value1 is a GDA_TYPE_NULL value and @value2 is of another type then the returned value is -1</para></listitem>
 *   <listitem><para>if @value1 is of another type and @value2 is a GDA_TYPE_NULL value then the returned value is 1</para></listitem>
 *   <listitem><para>in all other cases, @value1 and @value2 must be of the same type and their values are compared</para></listitem>
 * </itemizedlist>
 *
 * Returns: if both values have the same type, returns 0 if both contain
 * the same value, an integer less than 0 if @value1 is less than @value2 or
 * an integer greater than 0 if @value1 is greater than @value2.
 */
gint
gda_value_compare (const GValue *value1, const GValue *value2)
{
	GList *l1, *l2;
	gint retval;
	GType type;

	g_return_val_if_fail (value1 && value2, -1);

	type = G_VALUE_TYPE (value1);
	
	if (value1 == value2)
		return 0;

	/* handle GDA_TYPE_NULL comparisons with other types */
	else if (type == GDA_TYPE_NULL) {
		if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
			return 0;
		else
			return -1;
	}

	else if (G_VALUE_TYPE (value2) == GDA_TYPE_NULL)
		return 1;

	/* general case */
	g_return_val_if_fail (G_VALUE_TYPE (value1) == G_VALUE_TYPE (value2), -1);

	if (type == G_TYPE_INT64) {
		gint64 i1 = g_value_get_int64 (value1);
		gint64 i2 = g_value_get_int64 (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}
		
	else if (type == G_TYPE_UINT64) {
		guint64 i1 = g_value_get_uint64 (value1);
		guint64 i2 = g_value_get_uint64 (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == GDA_TYPE_BINARY) {
		const GdaBinary *binary1 = gda_value_get_binary (value1);
		const GdaBinary *binary2 = gda_value_get_binary (value2);
		if (binary1 && binary2 && (binary1->binary_length == binary2->binary_length))
			return memcmp (binary1->data, binary2->data, binary1->binary_length) ;
		else
			return -1;
	}

	else if (type == G_TYPE_BOOLEAN)
		return g_value_get_boolean (value1) - g_value_get_boolean (value2);

	else if (type == GDA_TYPE_BLOB) {
		const GdaBlob *blob1 = gda_value_get_blob (value1);
		const GdaBlob *blob2 = gda_value_get_blob (value2);
		if (blob1 && blob2 && (((GdaBinary *)blob1)->binary_length == ((GdaBinary *)blob2)->binary_length)) {
			if (blob1->op == blob2->op)
				return memcmp (((GdaBinary *)blob1)->data, ((GdaBinary *)blob2)->data, 
					       ((GdaBinary *)blob1)->binary_length);
			else
				return -1;
		}
		else
			return -1;
	}

	else if (type == G_TYPE_DATE) {
		GDate *d1, *d2;

		d1 = (GDate *) g_value_get_boxed (value1);
		d2 = (GDate *) g_value_get_boxed (value2);
		if (d1 && d2)
			return g_date_compare (d1, d2);
		else {
			if (d1)
				return 1;
			else {
				if (d2)
					return -1;
				else
					return 0;
			}
		}
	}

	else if (type == G_TYPE_DOUBLE) {
		gdouble v1, v2;

		v1 = g_value_get_double (value1);
		v2 = g_value_get_double (value2);
		
		if (v1 == v2)
			return 0;
		else
			return (v1 > v2) ? 1 : -1;
	}

	else if (type == GDA_TYPE_GEOMETRIC_POINT) {
		const GdaGeometricPoint *p1, *p2;
		p1 = gda_value_get_geometric_point (value1);
		p2 = gda_value_get_geometric_point (value2);
		if (p1 && p2)
			return memcmp (p1, p2, sizeof (GdaGeometricPoint));
		else if (p1)
			return 1;
		else if (p2)
			return -1;
		else
			return 0;
	}

	else if (type == G_TYPE_OBJECT) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	else if (type == G_TYPE_INT)
		return g_value_get_int (value1) - g_value_get_int (value2);

	else if (type == GDA_TYPE_LIST) {
		retval = 0;
		for (l1 = (GList*) gda_value_get_list (value1), l2 = (GList*) gda_value_get_list (value2); 
		     l1 != NULL && l2 != NULL; l1 = l1->next, l2 = l2->next){
			retval = gda_value_compare ((GValue *) l1->data,
						    (GValue *) l2->data);
			if (retval != 0) 
				return retval;
		}
		if (retval == 0 && (l1 == NULL || l2 == NULL) && l1 != l2)
			retval = (l1 == NULL) ? -1 : 1;
		
		return retval;
	}

	else if (type == GDA_TYPE_NUMERIC) {
		const GdaNumeric *num1, *num2;
		num1= gda_value_get_numeric (value1);
		num2 = gda_value_get_numeric (value2);
                if (num1) {
			if (num2)
				retval = strcmp (num1->number, num2->number);
			else
				retval = 1;
		}
		else {
			if (num2->number)
				retval = -1;
			else
				retval = 0;
		}
		return retval;
	}

	else if (type == G_TYPE_FLOAT) {
		gfloat f1 = g_value_get_float (value1);
		gfloat f2 = g_value_get_float (value2);
		return (f1 > f2) ? 1 : ((f1 == f2) ? 0 : -1);
	}

	else if (type == GDA_TYPE_SHORT) {
		gshort i1 = gda_value_get_short (value1);
		gshort i2 = gda_value_get_short (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_ULONG) {
		gulong i1 = g_value_get_ulong (value1);
		gulong i2 = g_value_get_ulong (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_LONG) {
		glong i1 = g_value_get_long (value1);
		glong i2 = g_value_get_long (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}
	
	else if (type == GDA_TYPE_USHORT) {
		gushort i1 = gda_value_get_ushort (value1);
		gushort i2 = gda_value_get_ushort (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}
	
	else if (type == G_TYPE_STRING)	{
		const gchar *str1, *str2;
		str1 = g_value_get_string (value1);
		str2 = g_value_get_string (value2);
		if (str1 && str2)
			retval = strcmp (str1, str2);
		else {
			if (str1)
				return 1;
			else {
				if (str2)
					return -1;
				else
					return 0;
			}
		}
		
		return retval;
	}
	
	else if (type == GDA_TYPE_TIME) {
		const GdaTime *t1, *t2;
		t1 = gda_value_get_time (value1);
		t2 = gda_value_get_time (value2);
		if (t1 && t2)
			return memcmp (t1, t2, sizeof (GdaTime));
		else if (t1)
			return 1;
		else if (t2)
			return -1;
		else
			return 0;
	}

	else if (type == GDA_TYPE_TIMESTAMP) {
		const GdaTimestamp *ts1, *ts2;
		ts1 = gda_value_get_timestamp (value1);
		ts2 = gda_value_get_timestamp (value2);
		if (ts1 && ts2)
			return memcmp (ts1, ts2, sizeof (GdaTimestamp));
		else if (ts1)
			return 1;
		else if (ts2)
			return -1;
		else
			return 0;
	}

	else if (type == G_TYPE_CHAR) {
		gchar c1 = g_value_get_char (value1);
		gchar c2 = g_value_get_char (value2);
		return (c1 > c2) ? 1 : ((c1 == c2) ? 0 : -1);
	}

	else if (type == G_TYPE_UCHAR) {
		guchar c1 = g_value_get_uchar (value1);
		guchar c2 = g_value_get_uchar (value2);
		return (c1 > c2) ? 1 : ((c1 == c2) ? 0 : -1);
	}

	else if (type == G_TYPE_UINT) {
		guint i1 = g_value_get_uint (value1);
		guint i2 = g_value_get_uint (value2);
		return (i1 > i2) ? 1 : ((i1 == i2) ? 0 : -1);
	}

	else if (type == G_TYPE_GTYPE) {
		GType t1 = g_value_get_gtype (value1);
		GType t2 = g_value_get_gtype (value2);
		return (t1 > t2) ? 1 : ((t1 == t2) ? 0: -1);
	}

	else if (g_type_is_a (type, G_TYPE_OBJECT)) {
		if (g_value_get_object (value1) == g_value_get_object (value2))
			return 0;
		else
			return -1;
	}

	g_warning ("%s() cannot handle values of type %s", __FUNCTION__, g_type_name (G_VALUE_TYPE (value1)));

	return 0;
}

/*
 * to_string
 * 
 * The exact reverse process of set_from_string(), almost the same as gda_value_stingify ()
 * because of some localization with gda_value_stingify ().
 */
static gchar *
to_string (const GValue *value)
{
	gchar *retval = NULL;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	if (G_VALUE_TYPE (value) == G_TYPE_BOOLEAN) {
		if (g_value_get_boolean (value))
			retval = g_strdup ("true");
		else
			retval = g_strdup ("false");
	}
	else
		retval = gda_value_stringify (value);
        	
	return retval;
}


/**
 * gda_value_to_xml: (skip)
 * @value: a #GValue.
 *
 * Serializes the given #GValue to an XML node string.
 *
 * Returns: (transfer full): the XML node. Once not needed anymore, you should free it.
 */
xmlNodePtr
gda_value_to_xml (const GValue *value)
{
	gchar *valstr;
	xmlNodePtr retval;

	g_return_val_if_fail (value && G_IS_VALUE (value), NULL);

	valstr = to_string (value);

	retval = xmlNewNode (NULL, (xmlChar*)"value");
	xmlSetProp(retval, (xmlChar*)"type", (xmlChar*)g_type_name (G_VALUE_TYPE (value)));
	xmlNodeSetContent (retval, (xmlChar*)valstr);

	g_free (valstr);

	return retval;
}


/* Register the GDA Types */

/* Gda gshort type */
/* Transform a String GValue to a gshort*/
static void 
string_to_short(const GValue *src, GValue *dest)
{
	
	const gchar *as_string;
	long int lvalue;
	gchar *endptr;

	g_return_if_fail (G_VALUE_HOLDS_STRING (src) &&
			  (GDA_VALUE_HOLDS_SHORT (dest) || GDA_VALUE_HOLDS_USHORT (dest)));
	
	as_string = g_value_get_string ((GValue *) src);
	
	lvalue = strtol (as_string, &endptr, 10);
	
	if (*as_string != '\0' && *endptr == '\0') {
		if (GDA_VALUE_HOLDS_SHORT (dest))
			gda_value_set_short (dest, (gshort) lvalue);
		else
			gda_value_set_ushort (dest, (gushort) lvalue);
	}
}

static void 
short_to_string (const GValue *src, GValue *dest) 
{
	gchar *str;
	
	g_return_if_fail (G_VALUE_HOLDS_STRING (dest) &&
			  (GDA_VALUE_HOLDS_SHORT (src) || GDA_VALUE_HOLDS_USHORT (src)));
	
	if (GDA_VALUE_HOLDS_SHORT (src))
		str = g_strdup_printf ("%d", gda_value_get_short ((GValue *) src));
	else
		str = g_strdup_printf ("%d", gda_value_get_ushort ((GValue *) src));
	
	g_value_take_string (dest, str);
}


GType
gda_short_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_INT, "GdaShort", &type_info, 0);		

		g_value_register_transform_func(G_TYPE_STRING,
						type,
						string_to_short);
		
		g_value_register_transform_func(type, 
						G_TYPE_STRING,
						short_to_string);  
	}
	  
	
	
  	return type;
}

GType
gda_ushort_get_type (void) {
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static const GTypeInfo type_info = {
    			0,			/* class_size */
    			NULL,		/* base_init */
    			NULL,		/* base_finalize */
    			NULL,		/* class_init */
    			NULL,		/* class_finalize */
    			NULL,		/* class_data */
    			0,			/* instance_size */
    			0,			/* n_preallocs */
    			NULL,		/* instance_init */
    			NULL		/* value_table */
  		};
  
  		type = g_type_register_static (G_TYPE_UINT, "GdaUShort", &type_info, 0);		
		
  		g_value_register_transform_func (G_TYPE_STRING,
						 type,
						 string_to_short);
		
		g_value_register_transform_func (type, 
						 G_TYPE_STRING,
						 short_to_string);
	}
	
  	return type;
}



#define MYMIN(a,b) (((b) > 0) ? ((a) < (b) ? (a) : (b)) : (a))

/**
 * gda_binary_to_string:
 * @bin: a correctly filled @GdaBinary structure
 * @maxlen: a maximum len used to truncate, or %0 for no maximum length
 *
 * Converts all the non printable characters of bin->data into the "\xyz" representation
 * where "xyz" is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\". Printable characters (defined by g_ascii_isprint()) as well as newline
 * character are not converted.
 *
 * Note that the backslash and newline characters are considered as printable characters and
 * will not be represented by the "\xyz" representation.
 *
 * Use this function to get a representation as much readable by humans as possible of a binary
 * chunk. Note that this function is internally called when transforming a binary value to
 * a string for example when using g_value_transform() or gda_value_stringify().
 *
 * Returns: (transfer full): a new string from @bin
 */
gchar *
gda_binary_to_string (const GdaBinary *bin, guint maxlen)
{
	gint nb_rewrites = 0;
	gchar *sptr, *rptr;
	gulong realsize = MYMIN (bin->binary_length, maxlen);

	gchar *retval;
	gulong offset = 0;

	if (!bin->data || (bin->binary_length == 0))
		return g_strdup ("");

	/* compute number of char rewrites */
	for (offset = 0, sptr = (gchar*) bin->data; 
	     offset < realsize; 
	     offset ++, sptr++) {
		if ((*sptr != '\n') && ((*sptr == '\\') || !g_ascii_isprint (*sptr)))
			nb_rewrites++;
	}
	
	/* mem allocation and copy */
	retval = g_malloc0 (realsize + nb_rewrites * 4 + 1);
	rptr = retval;
	sptr = (gchar*) bin->data;
	offset = 0;
	while (offset < realsize) {
		/* g_print (">%x<\n", (guchar) *ptr); */
		if ((*sptr == '\n') || ((*sptr != '\\') && g_ascii_isprint (*sptr))) {
			*rptr = *sptr;
			rptr ++;
		}
		else {
			if (*sptr == '\\') {
				*rptr = '\\';
				rptr++;
				*rptr = '\\';
				rptr++;
			}
			else {
				guchar val = *sptr;

				*rptr = '\\';
				rptr++;

				*rptr = val / 64 + '0';
				rptr++;
				val = val % 64;

				*rptr = val / 8 + '0';
				val = val % 8;
				rptr++;

				*rptr = val + '0';
				rptr++;
			}
		}

		sptr++;
		offset ++;
	}

	return retval;
}

/**
 * gda_string_to_binary:
 * @str: (allow-none): a string to convert, or %NULL
 *
 * Performs the reverse of gda_binary_to_string() (note that for any "\xyz" succession
 * of 4 characters where "xyz" represents a valid octal value, the resulting read value will
 * be modulo 256).
 *
 * I @str is %NULL, then an empty (i.e. where the @data part is %NULL) #GdaBinary is created and returned.
 *
 * Returns: (transfer full): a new #GdaBinary if no error were found in @str, or %NULL otherwise
 */
GdaBinary *
gda_string_to_binary (const gchar *str)
{
	GdaBinary *bin;
	glong len = 0, total;
	const guchar *sptr;
	guchar *rptr, *retval;

	if (!str) {
		bin = g_new0 (GdaBinary, 1);
		bin->data = NULL;
		bin->binary_length = 0;
		return bin;
	}

	total = strlen (str);
	retval = g_new0 (gchar, total + 1);
	sptr = (guchar*) str;
	rptr = retval;

	while (*sptr) {
		if (*sptr == '\\') {
			if (*(sptr+1) == '\\') {
				*rptr = '\\';
				sptr += 2;
			}
			else {
				guint tmp;
				if ((*(sptr+1) >= '0') && (*(sptr+1) <= '7') &&
				    (*(sptr+2) >= '0') && (*(sptr+2) <= '7') &&
				    (*(sptr+3) >= '0') && (*(sptr+3) <= '7')) {
					tmp = (*(sptr+1) - '0') * 64 +
						(*(sptr+2) - '0') * 8 +
						(*(sptr+3) - '0');
					sptr += 4;
					*rptr = tmp % 256;
				}
				else {
					g_free (retval);
					return NULL;
				}
			}
		}
		else {
			*rptr = *sptr;
			sptr++;
		}

		rptr++;
		len ++;
	}

	bin = g_new0 (GdaBinary, 1);
	bin->data = retval;
	bin->binary_length = len;

	return bin;
}

/**
 * gda_blob_to_string:
 * @blob: a correctly filled @GdaBlob structure
 * @maxlen: a maximum len used to truncate, or 0 for no maximum length
 *
 * Converts all the non printable characters of blob->data into the \xxx representation
 * where xxx is the octal representation of the byte, and the '\' (backslash) character
 * is converted to "\\".
 *
 * Returns: (transfer full): a new string from @blob
 */
gchar *
gda_blob_to_string (GdaBlob *blob, guint maxlen)
{
	if (!((GdaBinary *)blob)->data && blob->op) {
		/* fetch some data first (limited amount of data) */
		gda_blob_op_read (blob->op, blob, 0, 40);
	}
	return gda_binary_to_string ((GdaBinary*) blob, maxlen);
}

/**
 * gda_string_to_blob:
 * @str: a string to convert
 *
 * Performs the reverse of gda_blob_to_string().
 *
 * Returns: (transfer full): a new #gdaBlob if no error were found in @str, or NULL otherwise
 */
GdaBlob *
gda_string_to_blob (const gchar *str)
{
	GdaBinary *bin;
	bin = gda_string_to_binary (str);
	if (bin) {
		GdaBlob *blob;
		blob = g_new0 (GdaBlob, 1);
		((GdaBinary*) blob)->data = bin->data;
		((GdaBinary*) blob)->binary_length = bin->binary_length;
		blob->op = NULL;
		g_free (bin);
		return blob;
	}
	else
		return NULL;
}
