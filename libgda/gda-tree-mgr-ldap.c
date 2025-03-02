/*
 * Copyright (C) 2011 Vivien Malerba <malerba@gnome-db.org>
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

#include <glib/gi18n-lib.h>
#include <libgda/libgda.h>
#include "gda-tree-mgr-ldap.h"
#include "gda-tree-node.h"
#include <sqlite/virtual/gda-ldap-connection.h>

struct _GdaTreeMgrLdapPriv {
	GdaLdapConnection *cnc;
	gchar             *dn;
};

static void gda_tree_mgr_ldap_class_init (GdaTreeMgrLdapClass *klass);
static void gda_tree_mgr_ldap_init       (GdaTreeMgrLdap *tmgr1, GdaTreeMgrLdapClass *klass);
static void gda_tree_mgr_ldap_dispose    (GObject *object);
static void gda_tree_mgr_ldap_set_property (GObject *object,
					    guint param_id,
					    const GValue *value,
					    GParamSpec *pspec);
static void gda_tree_mgr_ldap_get_property (GObject *object,
					    guint param_id,
					    GValue *value,
					    GParamSpec *pspec);

/* virtual methods */
static GSList *gda_tree_mgr_ldap_update_children (GdaTreeManager *manager, GdaTreeNode *node, const GSList *children_nodes,
						  gboolean *out_error, GError **error);

static GObjectClass *parent_class = NULL;

/* properties */
enum {
        PROP_0,
	PROP_CNC,
	PROP_DN,
};

/*
 * GdaTreeMgrLdap class implementation
 * @klass:
 */
static void
gda_tree_mgr_ldap_class_init (GdaTreeMgrLdapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* virtual methods */
	((GdaTreeManagerClass*) klass)->update_children = gda_tree_mgr_ldap_update_children;

	/* Properties */
        object_class->set_property = gda_tree_mgr_ldap_set_property;
        object_class->get_property = gda_tree_mgr_ldap_get_property;

	/**
	 * GdaTreeMgrLdap:connection:
	 *
	 * Defines the #GdaLdapConnection to get information from.
	 */
	g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("connection", NULL, "Connection to use",
                                                              GDA_TYPE_LDAP_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	
	/**
	 * GdaTreeMgrLdap:dn:
	 *
	 * Defines the Distinguised Name of the LDAP entry to list children from
	 */
	g_object_class_install_property (object_class, PROP_DN,
                                         g_param_spec_string ("dn", NULL, "Distinguised Name",
							      NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
	object_class->dispose = gda_tree_mgr_ldap_dispose;
}

static void
gda_tree_mgr_ldap_init (GdaTreeMgrLdap *mgr, G_GNUC_UNUSED GdaTreeMgrLdapClass *klass)
{
	g_return_if_fail (GDA_IS_TREE_MGR_LDAP (mgr));
	mgr->priv = g_new0 (GdaTreeMgrLdapPriv, 1);
}

static void
gda_tree_mgr_ldap_dispose (GObject *object)
{
	GdaTreeMgrLdap *mgr = (GdaTreeMgrLdap *) object;

	g_return_if_fail (GDA_IS_TREE_MGR_LDAP (mgr));

	if (mgr->priv) {
		if (mgr->priv->cnc)
			g_object_unref (mgr->priv->cnc);
		g_free (mgr->priv->dn);
		g_free (mgr->priv);
		mgr->priv = NULL;
	}

	/* chain to parent class */
	parent_class->dispose (object);
}

/**
 * gda_tree_mgr_select_get_type:
 *
 * Returns: the GType
 *
 * Since: 4.2.8
 */
GType
gda_tree_mgr_ldap_get_type (void)
{
        static GType type = 0;

        if (G_UNLIKELY (type == 0)) {
                static GStaticMutex registering = G_STATIC_MUTEX_INIT;
                static const GTypeInfo info = {
                        sizeof (GdaTreeMgrLdapClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gda_tree_mgr_ldap_class_init,
                        NULL,
                        NULL,
                        sizeof (GdaTreeMgrLdap),
                        0,
                        (GInstanceInitFunc) gda_tree_mgr_ldap_init,
			0
                };

                g_static_mutex_lock (&registering);
                if (type == 0)
                        type = g_type_register_static (GDA_TYPE_TREE_MANAGER, "GdaTreeMgrLdap", &info, 0);
                g_static_mutex_unlock (&registering);
        }
        return type;
}

static void
gda_tree_mgr_ldap_set_property (GObject *object,
				guint param_id,
				const GValue *value,
				GParamSpec *pspec)
{
        GdaTreeMgrLdap *mgr;

        mgr = GDA_TREE_MGR_LDAP (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			mgr->priv->cnc = (GdaLdapConnection*) g_value_get_object (value);
			if (mgr->priv->cnc)
				g_object_ref (mgr->priv->cnc);
			break;
		case PROP_DN:
                        mgr->priv->dn = g_value_dup_string (value);
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

static void
gda_tree_mgr_ldap_get_property (GObject *object,
				guint param_id,
				GValue *value,
				GParamSpec *pspec)
{
        GdaTreeMgrLdap *mgr;

        mgr = GDA_TREE_MGR_LDAP (object);
        if (mgr->priv) {
                switch (param_id) {
		case PROP_CNC:
			g_value_set_object (value, mgr->priv->cnc);
			break;
		case PROP_DN:
			g_value_set_string (value, mgr->priv->dn);
                        break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
                }
        }
}

/**
 * gda_tree_mgr_ldap_new:
 * @cnc: a #GdaConnection object
 * @dn: (allow-none): an LDAP Distinguished Name or %NULL
 *
 * Creates a new #GdaTreeManager object which will list the children of the LDAP entry which Distinguished name
 * is @dn. If @dn is %NULL, then the tree manager will look in the tree itself for an attribute named "dn" and
 * use it.
 *
 * Returns: (transfer full): a new #GdaTreeManager object
 *
 * Since: 4.2.8
 */
GdaTreeManager*
gda_tree_mgr_ldap_new (GdaConnection *cnc, const gchar *dn)
{
	GdaTreeMgrLdap *mgr;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

	mgr = (GdaTreeMgrLdap*) g_object_new (GDA_TYPE_TREE_MGR_LDAP,
					      "connection", cnc, 
					      "dn", dn, NULL);
	return (GdaTreeManager*) mgr;
}

static GSList *
gda_tree_mgr_ldap_update_children (GdaTreeManager *manager, GdaTreeNode *node,
				   G_GNUC_UNUSED const GSList *children_nodes, gboolean *out_error,
				   GError **error)
{
	GdaTreeMgrLdap *mgr = GDA_TREE_MGR_LDAP (manager);
	gchar *real_dn = NULL;

	if (!mgr->priv->cnc) {
		g_set_error (error, GDA_TREE_MANAGER_ERROR, GDA_TREE_MANAGER_UNKNOWN_ERROR,
			     _("No LDAP connection specified"));
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}

	if (mgr->priv->dn)
		real_dn = g_strdup (mgr->priv->dn);
	else if (node) {
		/* looking for a dn in @node's attributes */
		const GValue *cvalue;
		cvalue = gda_tree_node_fetch_attribute (node, "dn");
		if (cvalue && (G_VALUE_TYPE (cvalue) == G_TYPE_STRING))
			real_dn = g_value_dup_string (cvalue);
	}

	GdaLdapEntry **entries;
	entries = gda_ldap_get_entry_children (mgr->priv->cnc, real_dn, NULL, error);
	g_free (real_dn);
	if (entries) {
		gint i;
		GSList *list = NULL;
		for (i = 0; entries [i]; i++) {
			GdaTreeNode* snode;
			GValue *dnv;
			GdaLdapEntry *lentry;
			lentry = entries [i];
			snode = gda_tree_manager_create_node (manager, node, lentry->dn);

			/* full DN */
			g_value_set_string ((dnv = gda_value_new (G_TYPE_STRING)), lentry->dn);
			gda_tree_node_set_node_attribute (snode, "dn", dnv, NULL);
			gda_value_free (dnv);

			/* RDN */
                        gchar **array;
                        array = gda_ldap_dn_split (lentry->dn, FALSE);
                        if (array) {
                                g_value_set_string ((dnv = gda_value_new (G_TYPE_STRING)), array [0]);
                                gda_tree_node_set_node_attribute (snode, "rdn", dnv, NULL);
				gda_value_free (dnv);
                                g_strfreev (array);
                        }

			if (gda_tree_manager_get_managers (manager)) {
				g_value_set_boolean ((dnv = gda_value_new (G_TYPE_BOOLEAN)), TRUE);
				gda_tree_node_set_node_attribute (snode,
								  GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN,
								  dnv, NULL);
				gda_value_free (dnv);
			}

			list = g_slist_prepend (list, snode);
			gda_ldap_entry_free (lentry);
		}
		g_free (entries);

		if (node)
			gda_tree_node_set_node_attribute (node,
							  GDA_ATTRIBUTE_TREE_NODE_UNKNOWN_CHILDREN,
							  NULL, NULL);
		return list;
	}
	else {
		if (out_error)
			*out_error = TRUE;
		return NULL;
	}
}
