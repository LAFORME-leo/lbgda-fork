/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __CLASS_PROPERTIES_H__
#define __CLASS_PROPERTIES_H__

#include "../browser-connection.h"

G_BEGIN_DECLS

#define CLASS_PROPERTIES_TYPE            (class_properties_get_type())
#define CLASS_PROPERTIES(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, CLASS_PROPERTIES_TYPE, ClassProperties))
#define CLASS_PROPERTIES_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, CLASS_PROPERTIES_TYPE, ClassPropertiesClass))
#define IS_CLASS_PROPERTIES(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, CLASS_PROPERTIES_TYPE))
#define IS_CLASS_PROPERTIES_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), CLASS_PROPERTIES_TYPE))

typedef struct _ClassProperties        ClassProperties;
typedef struct _ClassPropertiesClass   ClassPropertiesClass;
typedef struct _ClassPropertiesPrivate ClassPropertiesPrivate;

struct _ClassProperties {
	GtkVBox                 parent;
	ClassPropertiesPrivate *priv;
};

struct _ClassPropertiesClass {
	GtkVBoxClass            parent_class;

	/* signals */
	void                  (*open_class) (ClassProperties *eprop, const gchar *dn);
};

GType                    class_properties_get_type  (void) G_GNUC_CONST;

GtkWidget               *class_properties_new       (BrowserConnection *bcnc);
void                     class_properties_set_class (ClassProperties *eprop, const gchar *classname);

G_END_DECLS

#endif
