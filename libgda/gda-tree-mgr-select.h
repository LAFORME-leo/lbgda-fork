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

#ifndef __GDA_TREE_MGR_SELECT_H__
#define __GDA_TREE_MGR_SELECT_H__

#include <libgda/gda-connection.h>
#include "gda-tree-manager.h"

G_BEGIN_DECLS

#define GDA_TYPE_TREE_MGR_SELECT            (gda_tree_mgr_select_get_type())
#define GDA_TREE_MGR_SELECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_TREE_MGR_SELECT, GdaTreeMgrSelect))
#define GDA_TREE_MGR_SELECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_TREE_MGR_SELECT, GdaTreeMgrSelectClass))
#define GDA_IS_TREE_MGR_SELECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_TREE_MGR_SELECT))
#define GDA_IS_TREE_MGR_SELECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_TREE_MGR_SELECT))
#define GDA_TREE_MGR_SELECT_GET_CLASS(o)    (G_TYPE_INSTANCE_GET_CLASS ((o), GDA_TYPE_TREE_MGR_SELECT, GdaTreeMgrSelectClass))

typedef struct _GdaTreeMgrSelect GdaTreeMgrSelect;
typedef struct _GdaTreeMgrSelectPriv GdaTreeMgrSelectPriv;
typedef struct _GdaTreeMgrSelectClass GdaTreeMgrSelectClass;

struct _GdaTreeMgrSelect {
	GdaTreeManager        object;
	GdaTreeMgrSelectPriv *priv;
};

struct _GdaTreeMgrSelectClass {
	GdaTreeManagerClass   object_class;
};

GType              gda_tree_mgr_select_get_type                 (void) G_GNUC_CONST;
GdaTreeManager*    gda_tree_mgr_select_new                      (GdaConnection *cnc, GdaStatement *stmt, GdaSet *params);

G_END_DECLS

#endif
