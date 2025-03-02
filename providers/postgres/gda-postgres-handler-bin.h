/*
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2007 - 2008 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_POSTGRES_HANDLER_BIN__
#define __GDA_POSTGRES_HANDLER_BIN__

#include <libgda/gda-data-handler.h>

G_BEGIN_DECLS

#define GDA_TYPE_POSTGRES_HANDLER_BIN          (gda_postgres_handler_bin_get_type())
#define GDA_POSTGRES_HANDLER_BIN(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_POSTGRES_HANDLER_BIN, GdaPostgresHandlerBin)
#define GDA_POSTGRES_HANDLER_BIN_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_POSTGRES_HANDLER_BIN, GdaPostgresHandlerBinClass)
#define GDA_IS_POSTGRES_HANDLER_BIN(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_POSTGRES_HANDLER_BIN)


typedef struct _GdaPostgresHandlerBin      GdaPostgresHandlerBin;
typedef struct _GdaPostgresHandlerBinClass GdaPostgresHandlerBinClass;
typedef struct _GdaPostgresHandlerBinPriv  GdaPostgresHandlerBinPriv;


/* struct for the object's data */
struct _GdaPostgresHandlerBin
{
	GObject                     object;

	GdaPostgresHandlerBinPriv  *priv;
};

/* struct for the object's class */
struct _GdaPostgresHandlerBinClass
{
	GObjectClass                parent_class;
};


GType           gda_postgres_handler_bin_get_type      (void) G_GNUC_CONST;
GdaDataHandler *gda_postgres_handler_bin_new           (GdaConnection *cnc);

G_END_DECLS

#endif
