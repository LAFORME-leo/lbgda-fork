/*
 * Copyright (C) 2007 - 2009 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_VPROVIDER_DATA_MODEL_H__
#define __GDA_VPROVIDER_DATA_MODEL_H__

#include <virtual/gda-virtual-provider.h>

#define GDA_TYPE_VPROVIDER_DATA_MODEL            (gda_vprovider_data_model_get_type())
#define GDA_VPROVIDER_DATA_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_VPROVIDER_DATA_MODEL, GdaVproviderDataModel))
#define GDA_VPROVIDER_DATA_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_VPROVIDER_DATA_MODEL, GdaVproviderDataModelClass))
#define GDA_IS_VPROVIDER_DATA_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_VPROVIDER_DATA_MODEL))
#define GDA_IS_VPROVIDER_DATA_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_VPROVIDER_DATA_MODEL))

G_BEGIN_DECLS

typedef struct _GdaVproviderDataModel      GdaVproviderDataModel;
typedef struct _GdaVproviderDataModelClass GdaVproviderDataModelClass;
typedef struct _GdaVproviderDataModelPrivate GdaVproviderDataModelPrivate;

struct _GdaVproviderDataModel {
	GdaVirtualProvider            vprovider;
	GdaVproviderDataModelPrivate *priv;
};

struct _GdaVproviderDataModelClass {
	GdaVirtualProviderClass       parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType               gda_vprovider_data_model_get_type (void) G_GNUC_CONST;
GdaVirtualProvider *gda_vprovider_data_model_new      (void);

G_END_DECLS

#endif
