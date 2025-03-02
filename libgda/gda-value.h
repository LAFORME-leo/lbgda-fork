/*
 * Copyright (C) 2001 - 2003 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 - 2003 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2002 - 2010 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2003 Akira TAGOH <tagoh@gnome-db.org>
 * Copyright (C) 2003 Danilo Schoeneberg <dschoene@src.gnome.org>
 * Copyright (C) 2003 Laurent Sansonetti <laurent@datarescue.be>
 * Copyright (C) 2004 - 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2004 Paisa  Seeluangsawat <paisa@users.sf.net>
 * Copyright (C) 2008 Przemysław Grzegorczyk <pgrzegorczyk@gmail.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __GDA_VALUE_H__
#define __GDA_VALUE_H__

#include <time.h>
#include <glib.h>
#include <glib-object.h>
#include <libxml/tree.h>
#include <libgda/gda-decl.h>

G_BEGIN_DECLS

#define GDA_TIMEZONE_INVALID (2*12*60*60)

/* Definition of the GType's values used in GValue*/
#define GDA_TYPE_NULL 0
#define GDA_TYPE_DEFAULT (gda_default_get_type())
#define	GDA_TYPE_BINARY (gda_binary_get_type())
#define GDA_TYPE_BLOB (gda_blob_get_type())
#define	GDA_TYPE_GEOMETRIC_POINT (gda_geometricpoint_get_type())
#define	GDA_TYPE_LIST (gda_value_list_get_type())
#define	GDA_TYPE_NUMERIC (gda_numeric_get_type())
#define	GDA_TYPE_SHORT (gda_short_get_type()) 
#define	GDA_TYPE_USHORT (gda_ushort_get_type())
#define GDA_TYPE_TIME (gda_time_get_type())
#define GDA_TYPE_TIMESTAMP (gda_timestamp_get_type())


/* Definition of the GDA_VALUE_HOLDS macros */
#define GDA_VALUE_HOLDS_DEFAULT(value)         G_VALUE_HOLDS(value, GDA_TYPE_DEFAULT)
#define GDA_VALUE_HOLDS_BINARY(value)          G_VALUE_HOLDS(value, GDA_TYPE_BINARY)
#define GDA_VALUE_HOLDS_BLOB(value)            G_VALUE_HOLDS(value, GDA_TYPE_BLOB)
#define GDA_VALUE_HOLDS_GEOMETRIC_POINT(value) G_VALUE_HOLDS(value, GDA_TYPE_GEOMETRIC_POINT)
#define GDA_VALUE_HOLDS_LIST(value)            G_VALUE_HOLDS(value, GDA_TYPE_LIST)
#define GDA_VALUE_HOLDS_NUMERIC(value)         G_VALUE_HOLDS(value, GDA_TYPE_NUMERIC)
#define GDA_VALUE_HOLDS_SHORT(value)           G_VALUE_HOLDS(value, GDA_TYPE_SHORT)
#define GDA_VALUE_HOLDS_USHORT(value)          G_VALUE_HOLDS(value, GDA_TYPE_USHORT)
#define GDA_VALUE_HOLDS_TIME(value)            G_VALUE_HOLDS(value, GDA_TYPE_TIME)
#define GDA_VALUE_HOLDS_TIMESTAMP(value)       G_VALUE_HOLDS(value, GDA_TYPE_TIMESTAMP)

typedef struct {
	gdouble x;
	gdouble y;
} GdaGeometricPoint;

typedef struct {
	gchar   *number;
	glong    precision;
	glong    width;
	gpointer reserved; /* reserved for future usage with GMP (http://gmplib.org/) */
} GdaNumeric;

typedef struct {
	gushort hour;
	gushort minute;
	gushort second;
	gulong  fraction;
	glong   timezone;	/* # of seconds to the east UTC */
} GdaTime;

typedef struct {
	gshort  year;
	gushort month;
	gushort day;
	gushort hour;
	gushort minute;
	gushort second;
	gulong  fraction;
	glong   timezone;	/* # of seconds to the east UTC */
} GdaTimestamp;

typedef struct {
	guchar *data;
	glong   binary_length;
} GdaBinary;

/**
 * GdaBlob:
 * @data: data buffer, as a #GdaBinary
 * @op: (allow-none): a pointer to a #GdaBlopOp, or %NULL
 *
 * Represents some binary data, accessed through a #GdaBlobOp object.
 * @op is generally set up by database providers when giving access to an existing BLOB in
 * a database, but can be modified if needed using gda_blob_set_op().
 */
typedef struct {
	GdaBinary  data;
	GdaBlobOp *op;
} GdaBlob;

typedef GList GdaValueList;

#define gda_value_isa(value, type) (G_VALUE_HOLDS(value, type))

GValue                           *gda_value_new (GType type);
GValue                           *gda_value_new_default (const gchar *default_val);
GValue                           *gda_value_new_binary (const guchar *val, glong size);
GValue                           *gda_value_new_blob (const guchar *val, glong size);
GValue                           *gda_value_new_blob_from_file (const gchar *filename);
GValue                           *gda_value_new_timestamp_from_timet (time_t val);

GValue                           *gda_value_new_from_string (const gchar *as_string, GType type);
GValue                           *gda_value_new_from_xml (const xmlNodePtr node);

void                              gda_value_free (GValue *value);
void                              gda_value_reset_with_type (GValue *value, GType type);


gboolean                          gda_value_is_null (const GValue *value); 
gboolean                          gda_value_is_number (const GValue *value); 
GValue                           *gda_value_copy (const GValue *value);

const GdaBinary         *gda_value_get_binary (const GValue *value);
void                              gda_value_set_binary (GValue *value, const GdaBinary *binary);
void                              gda_value_take_binary (GValue *value, GdaBinary *binary);

const GdaBlob           *gda_value_get_blob (const GValue *value);
void                              gda_value_set_blob (GValue *value, const GdaBlob *blob);
void                              gda_value_take_blob (GValue *value, GdaBlob *blob);

const GdaGeometricPoint *gda_value_get_geometric_point (const GValue *value);
void                              gda_value_set_geometric_point (GValue *value, const GdaGeometricPoint *val);
const GdaValueList      *gda_value_get_list (const GValue *value);
void                              gda_value_set_list (GValue *value, const GdaValueList *val);
void                              gda_value_set_null (GValue *value);
const GdaNumeric        *gda_value_get_numeric (const GValue *value);
void                              gda_value_set_numeric (GValue *value, const GdaNumeric *val);
gshort                            gda_value_get_short (const GValue *value);
void                              gda_value_set_short (GValue *value, const gshort val);
gushort                           gda_value_get_ushort (const GValue *value);
void                              gda_value_set_ushort (GValue *value, const gushort val);
const GdaTime           *gda_value_get_time (const GValue *value);
void                              gda_value_set_time (GValue *value, const GdaTime *val);
const GdaTimestamp      *gda_value_get_timestamp (const GValue *value);
void                              gda_value_set_timestamp (GValue *value, const GdaTimestamp *val);




gboolean                          gda_value_set_from_string (GValue *value, 
						             const gchar *as_string,
						             GType type);
gboolean                          gda_value_set_from_value (GValue *value, const GValue *from);

gint                              gda_value_differ (const GValue *value1, const GValue *value2);
gint                              gda_value_compare (const GValue *value1, const GValue *value2);
gchar                            *gda_value_stringify (const GValue *value);
xmlNodePtr                        gda_value_to_xml (const GValue *value);

gchar                            *gda_binary_to_string (const GdaBinary *bin, guint maxlen);
GdaBinary                        *gda_string_to_binary (const gchar *str);

gchar                            *gda_blob_to_string (GdaBlob *blob, guint maxlen);
GdaBlob                          *gda_string_to_blob (const gchar *str);

/* Custom data types */

GType                             gda_default_get_type (void) G_GNUC_CONST;
GType                             gda_numeric_get_type (void) G_GNUC_CONST;
gpointer                          gda_numeric_copy (gpointer boxed);
void                              gda_numeric_free (gpointer boxed);

GType                             gda_time_get_type (void) G_GNUC_CONST;
gpointer                          gda_time_copy (gpointer boxed);
void                              gda_time_free (gpointer boxed);
gboolean                          gda_time_valid (const GdaTime *time);


GType                             gda_timestamp_get_type (void) G_GNUC_CONST;
gpointer                          gda_timestamp_copy (gpointer boxed);
void                              gda_timestamp_free (gpointer boxed);
gboolean                          gda_timestamp_valid (const GdaTimestamp *timestamp);


GType                             gda_geometricpoint_get_type (void) G_GNUC_CONST;
gpointer                          gda_geometricpoint_copy (gpointer boxed);
void                              gda_geometricpoint_free (gpointer boxed);

GType                             gda_binary_get_type (void) G_GNUC_CONST;
gpointer                          gda_binary_copy (gpointer boxed);
void                              gda_binary_free (gpointer boxed);

GType                             gda_blob_get_type (void) G_GNUC_CONST;
gpointer                          gda_blob_copy (gpointer boxed);
void                              gda_blob_free (gpointer boxed);
void                              gda_blob_set_op (GdaBlob *blob, GdaBlobOp *op);

GType                             gda_value_list_get_type (void) G_GNUC_CONST;
GType                             gda_short_get_type (void) G_GNUC_CONST;
GType                             gda_ushort_get_type (void) G_GNUC_CONST;

/* Helper macros */
#define                           gda_value_new_null() (g_new0 (GValue, 1))

G_END_DECLS

#endif
