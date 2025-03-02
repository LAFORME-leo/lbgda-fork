/*
 * Copyright (C) 2010 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDAUI_RT_EDITOR__
#define __GDAUI_RT_EDITOR__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define GDAUI_TYPE_RT_EDITOR          (gdaui_rt_editor_get_type())
#define GDAUI_RT_EDITOR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gdaui_rt_editor_get_type(), GdauiRtEditor)
#define GDAUI_RT_EDITOR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gdaui_rt_editor_get_type (), GdauiRtEditorClass)
#define GDAUI_IS_RT_EDITOR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gdaui_rt_editor_get_type ())


typedef struct _GdauiRtEditor      GdauiRtEditor;
typedef struct _GdauiRtEditorClass GdauiRtEditorClass;
typedef struct _GdauiRtEditorPriv  GdauiRtEditorPriv;

/* struct for the object's data */
struct _GdauiRtEditor
{
	GtkVBox              object;

	GdauiRtEditorPriv   *priv;
};

/* struct for the object's class */
struct _GdauiRtEditorClass
{
	GtkVBoxClass         parent_class;

	/* signals */
        void (* changed) (GdauiRtEditor *editor);
};

/* 
 * Generic widget's methods 
 */
GType      gdaui_rt_editor_get_type              (void) G_GNUC_CONST;

GtkWidget *gdaui_rt_editor_new                   (void);
gchar     *gdaui_rt_editor_get_contents          (GdauiRtEditor *editor);
void       gdaui_rt_editor_set_contents          (GdauiRtEditor *editor, const gchar *markup, gint length);
void       gdaui_rt_editor_set_editable          (GdauiRtEditor *editor, gboolean editable);

G_END_DECLS

#endif



