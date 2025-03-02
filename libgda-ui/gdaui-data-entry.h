/*
 * Copyright (C) 2009 - 2010 Vivien Malerba <malerba@gnome-db.org>
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


#ifndef __GDAUI_DATA_ENTRY_H_
#define __GDAUI_DATA_ENTRY_H_

#include <glib-object.h>
#include <libgda/libgda.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_ENTRY          (gdaui_data_entry_get_type())
#define GDAUI_DATA_ENTRY(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DATA_ENTRY, GdauiDataEntry)
#define GDAUI_IS_DATA_ENTRY(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DATA_ENTRY)
#define GDAUI_DATA_ENTRY_GET_IFACE(obj) (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDAUI_TYPE_DATA_ENTRY, GdauiDataEntryIface))

typedef struct _GdauiDataEntry        GdauiDataEntry;
typedef struct _GdauiDataEntryIface   GdauiDataEntryIface;

/* error reporting */
extern GQuark gdaui_data_entry_error_quark (void);
#define GDAUI_DATA_ENTRY_ERROR gdaui_data_entry_error_quark ()

typedef enum
{
	GDAUI_DATA_ENTRY_FILE_NOT_FOUND_ERROR,
	GDAUI_DATA_ENTRY_INVALID_DATA_ERROR
} GdauiDataEntryError;

/* struct for the interface */
struct _GdauiDataEntryIface
{
	GTypeInterface           g_iface;

	/* signals */
	void            (* contents_modified)    (GdauiDataEntry *de);
	void            (* contents_activated)   (GdauiDataEntry *de);
	void            (* status_changed)       (GdauiDataEntry *de);
	gboolean        (* contents_valid)       (GdauiDataEntry *de, GError **error);

	/* virtual table */
	void            (*set_value_type)        (GdauiDataEntry *de, GType type);
	GType           (*get_value_type)        (GdauiDataEntry *de);
	void            (*set_value)             (GdauiDataEntry *de, const GValue * value);
	GValue         *(*get_value)             (GdauiDataEntry *de);
	void            (*set_ref_value)         (GdauiDataEntry *de, const GValue * value);
	const GValue   *(*get_ref_value)         (GdauiDataEntry *de);
	void            (*set_value_default)     (GdauiDataEntry *de, const GValue * value);
	void            (*set_attributes)        (GdauiDataEntry *de, GdaValueAttribute attrs, GdaValueAttribute mask);
	GdaValueAttribute (*get_attributes)      (GdauiDataEntry *de);
	GdaDataHandler *(*get_handler)           (GdauiDataEntry *de);
	gboolean        (*can_expand)            (GdauiDataEntry *de, gboolean horiz);
	void            (*set_editable)          (GdauiDataEntry *de, gboolean editable);
	gboolean        (*get_editable)          (GdauiDataEntry *de);
	void            (*grab_focus)            (GdauiDataEntry *de);

	/* another signal */
	void            (*expand_changed)        (GdauiDataEntry *de);

	void            (*set_unknown_color)     (GdauiDataEntry *de, gdouble red, gdouble green,
						  gdouble blue, gdouble alpha);

	/*< private >*/
	/* Padding for future expansion */
        void (*_gdaui_reserved3) (void);
        void (*_gdaui_reserved4) (void);
};




GType           gdaui_data_entry_get_type               (void) G_GNUC_CONST;

void            gdaui_data_entry_set_value_type         (GdauiDataEntry *de, GType type);
GType           gdaui_data_entry_get_value_type         (GdauiDataEntry *de);

void            gdaui_data_entry_set_value              (GdauiDataEntry *de, const GValue *value);
GValue         *gdaui_data_entry_get_value              (GdauiDataEntry *de);
gboolean        gdaui_data_entry_content_is_valid       (GdauiDataEntry *de, GError **error);
void            gdaui_data_entry_set_reference_value    (GdauiDataEntry *de, const GValue *value);
const GValue   *gdaui_data_entry_get_reference_value    (GdauiDataEntry *de);
void            gdaui_data_entry_set_reference_current  (GdauiDataEntry *de);
void            gdaui_data_entry_set_default_value      (GdauiDataEntry *de, const GValue *value);

void            gdaui_data_entry_set_attributes         (GdauiDataEntry *de, GdaValueAttribute attrs,
							 GdaValueAttribute mask);
GdaValueAttribute gdaui_data_entry_get_attributes       (GdauiDataEntry *de);

GdaDataHandler *gdaui_data_entry_get_handler            (GdauiDataEntry *de);
gboolean        gdaui_data_entry_can_expand             (GdauiDataEntry *de, gboolean horiz);
void            gdaui_data_entry_set_editable           (GdauiDataEntry *de, gboolean editable);
gboolean        gdaui_data_entry_get_editable           (GdauiDataEntry *de);
void            gdaui_data_entry_grab_focus             (GdauiDataEntry *de);

void            gdaui_data_entry_set_unknown_color      (GdauiDataEntry *de, gdouble red, gdouble green,
						 	 gdouble blue, gdouble alpha);

G_END_DECLS

#endif
