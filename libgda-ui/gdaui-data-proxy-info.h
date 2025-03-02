/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_DATA_PROXY_INFO__
#define __GDAUI_DATA_PROXY_INFO__

#include <gtk/gtk.h>
#include <libgda/gda-decl.h>
#include <libgda-ui/gdaui-decl.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_DATA_PROXY_INFO          (gdaui_data_proxy_info_get_type())
#define GDAUI_DATA_PROXY_INFO(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_data_proxy_info_get_type(), GdauiDataProxyInfo)
#define GDAUI_DATA_PROXY_INFO_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_data_proxy_info_get_type (), GdauiDataProxyInfoClass)
#define GDAUI_IS_DATA_PROXY_INFO(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_data_proxy_info_get_type ())


typedef struct _GdauiDataProxyInfo      GdauiDataProxyInfo;
typedef struct _GdauiDataProxyInfoClass GdauiDataProxyInfoClass;
typedef struct _GdauiDataProxyInfoPriv  GdauiDataProxyInfoPriv;

typedef enum 
{
	GDAUI_DATA_PROXY_INFO_NONE = 0,
	GDAUI_DATA_PROXY_INFO_CURRENT_ROW    = 1 << 0,
	GDAUI_DATA_PROXY_INFO_ROW_MODIFY_BUTTONS = 1 << 2,
	GDAUI_DATA_PROXY_INFO_ROW_MOVE_BUTTONS = 1 << 3,
	GDAUI_DATA_PROXY_INFO_CHUNCK_CHANGE_BUTTONS = 1 << 4,
	GDAUI_DATA_PROXY_INFO_NO_FILTER = 1 << 5
} GdauiDataProxyInfoFlag;

/* struct for the object's data */
struct _GdauiDataProxyInfo
{
	GtkHBox                 object;

	GdauiDataProxyInfoPriv *priv;
};

/* struct for the object's class */
struct _GdauiDataProxyInfoClass
{
	GtkHBoxClass            parent_class;
};

/* 
 * Generic widget's methods 
 */
GType             gdaui_data_proxy_info_get_type (void) G_GNUC_CONST;
GtkWidget        *gdaui_data_proxy_info_new      (GdauiDataProxy *data_proxy, GdauiDataProxyInfoFlag flags);

G_END_DECLS

#endif



