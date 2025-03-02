/*
 * Copyright (C) 2006 - 2009 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
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

#ifndef __GDA_HANDLER_STRING__
#define __GDA_HANDLER_STRING__

#include <glib-object.h>
#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_HANDLER_STRING          (gda_handler_string_get_type())
#define GDA_HANDLER_STRING(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_handler_string_get_type(), GdaHandlerString)
#define GDA_HANDLER_STRING_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_handler_string_get_type (), GdaHandlerStringClass)
#define GDA_IS_HANDLER_STRING(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_handler_string_get_type ())

typedef struct _GdaHandlerString      GdaHandlerString;
typedef struct _GdaHandlerStringClass GdaHandlerStringClass;
typedef struct _GdaHandlerStringPriv  GdaHandlerStringPriv;

/* struct for the object's data */
struct _GdaHandlerString
{
	GObject                object;
	GdaHandlerStringPriv  *priv;
};

/* struct for the object's class */
struct _GdaHandlerStringClass
{
	GObjectClass           parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
};


GType           gda_handler_string_get_type          (void) G_GNUC_CONST;
GdaDataHandler *gda_handler_string_new               (void);
GdaDataHandler *gda_handler_string_new_with_provider (GdaServerProvider *prov, GdaConnection *cnc);

G_END_DECLS

#endif
