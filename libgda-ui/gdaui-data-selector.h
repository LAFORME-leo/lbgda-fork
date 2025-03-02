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


#ifndef __GDAUI_DATA_SELECTOR_H_
#define __GDAUI_DATA_SELECTOR_H_

#include <glib-object.h>
#include <gtk/gtk.h>
#include <libgda/libgda.h>
#include "gdaui-decl.h"

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_SELECTOR          (gdaui_data_selector_get_type())
#define GDAUI_DATA_SELECTOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDAUI_TYPE_DATA_SELECTOR, GdauiDataSelector)
#define GDAUI_IS_DATA_SELECTOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDAUI_TYPE_DATA_SELECTOR)
#define GDAUI_DATA_SELECTOR_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), GDAUI_TYPE_DATA_SELECTOR, GdauiDataSelectorIface))

typedef struct _GdauiDataSelectorIface GdauiDataSelectorIface;
typedef struct _GdauiDataSelector GdauiDataSelector;

/* struct for the interface */
struct _GdauiDataSelectorIface
{
	GTypeInterface           g_iface;

	/* virtual table */
	GdaDataModel     *(*get_model)             (GdauiDataSelector *iface);
	void              (*set_model)             (GdauiDataSelector *iface, GdaDataModel *model);
	GArray           *(*get_selected_rows)     (GdauiDataSelector *iface);
	GdaDataModelIter *(*get_data_set)          (GdauiDataSelector *iface);
	gboolean          (*select_row)            (GdauiDataSelector *iface, gint row);
	void              (*unselect_row)          (GdauiDataSelector *iface, gint row);
	void              (*set_column_visible)    (GdauiDataSelector *iface, gint column, gboolean visible);

	/* signals */
	void              (* selection_changed)    (GdauiDataSelector *iface);
};

GType             gdaui_data_selector_get_type              (void) G_GNUC_CONST;

GdaDataModel     *gdaui_data_selector_get_model             (GdauiDataSelector *iface);
void              gdaui_data_selector_set_model             (GdauiDataSelector *iface, GdaDataModel *model);
GArray           *gdaui_data_selector_get_selected_rows     (GdauiDataSelector *iface);
GdaDataModelIter *gdaui_data_selector_get_data_set          (GdauiDataSelector *iface);
gboolean          gdaui_data_selector_select_row            (GdauiDataSelector *iface, gint row);
void              gdaui_data_selector_unselect_row          (GdauiDataSelector *iface, gint row);
void              gdaui_data_selector_set_column_visible    (GdauiDataSelector *iface, gint column, gboolean visible);

G_END_DECLS

#endif
