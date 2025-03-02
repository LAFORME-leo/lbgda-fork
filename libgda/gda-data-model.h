/*
 * Copyright (C) 2001 - 2004 Rodrigo Moya <rodrigo@gnome-db.org>
 * Copyright (C) 2002 Gonzalo Paniagua Javier <gonzalo@gnome-db.org>
 * Copyright (C) 2003 Laurent Sansonetti <lrz@gnome.org>
 * Copyright (C) 2005 Dan Winship <danw@src.gnome.org>
 * Copyright (C) 2005 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2005 �lvaro Pe�a <alvaropg@telefonica.net>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
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

#ifndef __GDA_DATA_MODEL_H__
#define __GDA_DATA_MODEL_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-column.h>
#include <libgda/gda-value.h>
#include <libgda/gda-enums.h>

G_BEGIN_DECLS

#define GDA_TYPE_DATA_MODEL            (gda_data_model_get_type())
#define GDA_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_DATA_MODEL, GdaDataModel))
#define GDA_IS_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_DATA_MODEL))
#define GDA_DATA_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDA_TYPE_DATA_MODEL, GdaDataModelIface))

/* error reporting */
extern GQuark gda_data_model_error_quark (void);
#define GDA_DATA_MODEL_ERROR gda_data_model_error_quark ()

typedef enum {
	GDA_DATA_MODEL_ACCESS_RANDOM = 1 << 0,
	GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD = 1 << 1,
	GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD = 1 << 2,
	GDA_DATA_MODEL_ACCESS_CURSOR = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD | GDA_DATA_MODEL_ACCESS_CURSOR_BACKWARD,
	GDA_DATA_MODEL_ACCESS_INSERT  = 1 << 3,
	GDA_DATA_MODEL_ACCESS_UPDATE  = 1 << 4,
	GDA_DATA_MODEL_ACCESS_DELETE  = 1 << 5,
	GDA_DATA_MODEL_ACCESS_WRITE = GDA_DATA_MODEL_ACCESS_INSERT | GDA_DATA_MODEL_ACCESS_UPDATE |
	GDA_DATA_MODEL_ACCESS_DELETE
} GdaDataModelAccessFlags;

typedef enum {
	GDA_DATA_MODEL_HINT_START_BATCH_UPDATE,
	GDA_DATA_MODEL_HINT_END_BATCH_UPDATE,
	GDA_DATA_MODEL_HINT_REFRESH
} GdaDataModelHint;

typedef enum {
	GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
	GDA_DATA_MODEL_IO_TEXT_SEPARATED
} GdaDataModelIOFormat;

typedef enum {
	GDA_DATA_MODEL_ROW_OUT_OF_RANGE_ERROR,
	GDA_DATA_MODEL_COLUMN_OUT_OF_RANGE_ERROR,
	GDA_DATA_MODEL_VALUES_LIST_ERROR,
	GDA_DATA_MODEL_VALUE_TYPE_ERROR,
	GDA_DATA_MODEL_ROW_NOT_FOUND_ERROR,
	GDA_DATA_MODEL_ACCESS_ERROR,
	GDA_DATA_MODEL_FEATURE_NON_SUPPORTED_ERROR,
	GDA_DATA_MODEL_FILE_EXIST_ERROR,
	GDA_DATA_MODEL_XML_FORMAT_ERROR,

	GDA_DATA_MODEL_TRUNCATED_ERROR,
	GDA_DATA_MODEL_OTHER_ERROR
} GdaDataModelError;

/* struct for the interface */
struct _GdaDataModelIface {
	GTypeInterface           g_iface;

	/* virtual table */
	gint                 (* i_get_n_rows)       (GdaDataModel *model);
	gint                 (* i_get_n_columns)    (GdaDataModel *model);

	GdaColumn           *(* i_describe_column)  (GdaDataModel *model, gint col);
	GdaDataModelAccessFlags (* i_get_access_flags) (GdaDataModel *model);

	const GValue        *(* i_get_value_at)     (GdaDataModel *model, gint col, gint row, GError **error);
	GdaValueAttribute    (* i_get_attributes_at)(GdaDataModel *model, gint col, gint row);
	GdaDataModelIter    *(* i_create_iter)      (GdaDataModel *model);
	gboolean             (* i_iter_at_row)      (GdaDataModel *model, GdaDataModelIter *iter, gint row);
	gboolean             (* i_iter_next)        (GdaDataModel *model, GdaDataModelIter *iter); 
	gboolean             (* i_iter_prev)        (GdaDataModel *model, GdaDataModelIter *iter);

	gboolean             (* i_set_value_at)     (GdaDataModel *model, gint col, gint row, 
						     const GValue *value, GError **error);
	gboolean             (* i_iter_set_value)   (GdaDataModel *model, GdaDataModelIter *iter, gint col,
						     const GValue *value, GError **error);
	gboolean             (* i_set_values)       (GdaDataModel *model, gint row, GList *values,
						     GError **error);
	gint                 (* i_append_values)    (GdaDataModel *model, const GList *values, GError **error);
	gint                 (* i_append_row)       (GdaDataModel *model, GError **error);
	gboolean             (* i_remove_row)       (GdaDataModel *model, gint row, GError **error);
	gint                 (* i_find_row)         (GdaDataModel *model, GSList *values, gint *cols_index);

	void                 (* i_set_notify)       (GdaDataModel *model, gboolean do_notify_changes);
	gboolean             (* i_get_notify)       (GdaDataModel *model);
	void                 (* i_send_hint)        (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value);

	/* signals */
	void                 (* row_inserted)       (GdaDataModel *model, gint row);
	void                 (* row_updated)        (GdaDataModel *model, gint row);
	void                 (* row_removed)        (GdaDataModel *model, gint row);
	void                 (* changed)            (GdaDataModel *model);
	void                 (* reset)              (GdaDataModel *model);
	void                 (* access_changed)     (GdaDataModel *model);

	/* getting more information about a data model */
	GError             **(* i_get_exceptions)   (GdaDataModel *model);
};

GType               gda_data_model_get_type               (void) G_GNUC_CONST;

GdaDataModelAccessFlags gda_data_model_get_access_flags   (GdaDataModel *model);

gint                gda_data_model_get_n_rows             (GdaDataModel *model);
gint                gda_data_model_get_n_columns          (GdaDataModel *model);

GdaColumn          *gda_data_model_describe_column        (GdaDataModel *model, gint col);
gint                gda_data_model_get_column_index       (GdaDataModel *model, const gchar *name);
const gchar        *gda_data_model_get_column_name       (GdaDataModel *model, gint col);
void                gda_data_model_set_column_name       (GdaDataModel *model, gint col, const gchar *name);
const gchar        *gda_data_model_get_column_title       (GdaDataModel *model, gint col);
void                gda_data_model_set_column_title       (GdaDataModel *model, gint col, const gchar *title);

const GValue       *gda_data_model_get_value_at           (GdaDataModel *model, gint col, gint row, GError **error);
const GValue       *gda_data_model_get_typed_value_at     (GdaDataModel *model, gint col, gint row, 
							   GType expected_type, gboolean nullok, GError **error);
GdaValueAttribute   gda_data_model_get_attributes_at      (GdaDataModel *model, gint col, gint row);
GdaDataModelIter   *gda_data_model_create_iter            (GdaDataModel *model);
void                gda_data_model_freeze                 (GdaDataModel *model);
void                gda_data_model_thaw                   (GdaDataModel *model);
gboolean            gda_data_model_set_value_at           (GdaDataModel *model, gint col, gint row, 
							   const GValue *value, GError **error);
gboolean            gda_data_model_set_values             (GdaDataModel *model, gint row, 
							   GList *values, GError **error);
gint                gda_data_model_append_row             (GdaDataModel *model, GError **error);
gint                gda_data_model_append_values          (GdaDataModel *model, const GList *values, GError **error);
gboolean            gda_data_model_remove_row             (GdaDataModel *model, gint row, GError **error);
gint                gda_data_model_get_row_from_values    (GdaDataModel *model, GSList *values, gint *cols_index);

void                gda_data_model_send_hint              (GdaDataModel *model, GdaDataModelHint hint, const GValue *hint_value);

GError            **gda_data_model_get_exceptions         (GdaDataModel *model);

/* contents saving and loading */
gchar              *gda_data_model_export_to_string       (GdaDataModel *model, GdaDataModelIOFormat format, 
							   const gint *cols, gint nb_cols, 
							   const gint *rows, gint nb_rows, GdaSet *options);
gboolean            gda_data_model_export_to_file         (GdaDataModel *model, GdaDataModelIOFormat format, 
							   const gchar *file,
							   const gint *cols, gint nb_cols, 
							   const gint *rows, gint nb_rows, 
							   GdaSet *options, GError **error);

gboolean            gda_data_model_import_from_model      (GdaDataModel *to, GdaDataModel *from, gboolean overwrite,
							   GHashTable *cols_trans, GError **error);
gboolean            gda_data_model_import_from_string     (GdaDataModel *model,
							   const gchar *string, GHashTable *cols_trans,
							   GdaSet *options, GError **error);
gboolean            gda_data_model_import_from_file       (GdaDataModel *model,
							   const gchar *file, GHashTable *cols_trans,
							   GdaSet *options, GError **error);

/* debug functions */
void                gda_data_model_dump                   (GdaDataModel *model, FILE *to_stream);
gchar              *gda_data_model_dump_as_string         (GdaDataModel *model);

G_END_DECLS

#endif
