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

#include <string.h>
#include <glib/gi18n-lib.h>
#include <libgda/gda-data-model-ldap.h>
#include <libgda/gda-connection.h>
#include <libgda/gda-data-model-iter.h>
#include <libgda/gda-holder.h>
#include <libgda/gda-util.h>
#include <libgda/sqlite/virtual/gda-virtual-connection.h>
#include <libgda/sqlite/virtual/gda-ldap-connection.h>
#include "gda-ldap.h"
#include "gda-ldap-util.h"
#include "gdaprov-data-model-ldap.h"

#define GDA_DEBUG_SUBSEARCHES
#undef GDA_DEBUG_SUBSEARCHES

/*
 * What to do in case of a multi value in a cell
 */
typedef enum {
	MULTIPLE_VALUE_ACTION_SET_NULL,
	MULTIPLE_VALUE_ACTION_CSV_STRING,
	MULTIPLE_VALUE_ACTION_MULTIPLY,
	MULTIPLE_VALUE_ACTION_SET_INVALID,
	MULTIPLE_VALUE_ACTION_FIRST,
	MULTIPLE_VALUE_ACTION_CONCAT
} MultipleValueAction;

typedef struct _LdapPart LdapPart;
struct _LdapPart {
	gchar           *base_dn;
	GdaLdapSearchScope scope;
	gboolean         executed; /* %TRUE if @ldap_msg of @children have already had the
				    * opportunity of being computed */

	/* result of execution, both %NULL if data was truncated when executing */
	LDAPMessage     *ldap_msg;
	gint             nb_entries;
	LDAPMessage     *ldap_row; /* no ref! */

	/* tree-like structure */
	GSList          *children; /* list of #LdapPart, ref held there */
	LdapPart        *parent; /* no ref held */
};
#define LDAP_PART(x) ((LdapPart*)(x))

static LdapPart *ldap_part_new (LdapPart *parent, const gchar *base_dn, GdaLdapSearchScope scope);
static void ldap_part_free (LdapPart *part);
static gboolean ldap_part_split (LdapPart *part, GdaDataModelLdap *model, gboolean *out_error);
static LdapPart *ldap_part_next (LdapPart *part, gboolean executed);
#ifdef GDA_DEBUG_SUBSEARCHES
static void ldap_part_dump (LdapPart *part);
#endif

static void add_exception (GdaDataModelLdap *model, GError *e);

typedef struct {
	GdaHolder *holder;
	gint       index;
	GArray    *values; /* array of #GValue, or %NULL on error */
} ColumnMultiplier;
static ColumnMultiplier *column_multiplier_new (GdaHolder *holder,
						const GValue *value);

typedef struct {
	GArray *cms; /* array of ColumnMultiplier pointers, stores in reverse order */
} RowMultiplier;
static RowMultiplier *row_multiplier_new (void);
static gboolean row_multiplier_index_next (RowMultiplier *rm);
static void row_multiplier_free (RowMultiplier *rm);
static void row_multiplier_set_holders (RowMultiplier *rm);

struct _GdaDataModelLdapPrivate {
	GdaConnection      *cnc;
	gchar              *base_dn;
	gboolean            use_rdn;
	gchar              *filter;
	GArray             *attributes;
	GdaLdapSearchScope  scope;
	MultipleValueAction default_mv_action;
	GList              *columns;
	GArray             *column_mv_actions; /* array of #MultipleValueAction, notincluding column 0 */
	gint                n_columns; /* length of @columns */
	gint                n_rows;
	gboolean            truncated;

	gint                iter_row;
	LdapPart           *top_exec; /* ref held */
	LdapPart           *current_exec; /* no ref held, only a pointer in the @top_exec tree */

	RowMultiplier      *row_mult;

	GArray             *exceptions; /* array of GError pointers */
};

/* properties */
enum {
	PROP_0,
	PROP_CNC,
	PROP_BASE,
	PROP_FILTER,
	PROP_ATTRIBUTES,
	PROP_SCOPE,
	PROP_USE_RDN
};

static void gda_data_model_ldap_class_init (GdaDataModelLdapClass *klass);
static void gda_data_model_ldap_init       (GdaDataModelLdap *model,
					    GdaDataModelLdapClass *klass);
static void gda_data_model_ldap_dispose    (GObject *object);

static void gda_data_model_ldap_set_property (GObject *object,
					      guint param_id,
					      const GValue *value,
					      GParamSpec *pspec);
static void gda_data_model_ldap_get_property (GObject *object,
					      guint param_id,
					      GValue *value,
					      GParamSpec *pspec);

static GList *_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes,
				     GArray **out_attrs_array,
				     MultipleValueAction default_mva, GArray **out_mv_actions);

/* GdaDataModel interface */
static void                 gda_data_model_ldap_data_model_init (GdaDataModelIface *iface);
static gint                 gda_data_model_ldap_get_n_rows      (GdaDataModel *model);
static gint                 gda_data_model_ldap_get_n_columns   (GdaDataModel *model);
static GdaColumn           *gda_data_model_ldap_describe_column (GdaDataModel *model, gint col);
static GdaDataModelAccessFlags gda_data_model_ldap_get_access_flags(GdaDataModel *model);
static gboolean             gda_data_model_ldap_iter_next       (GdaDataModel *model, GdaDataModelIter *iter);
static GdaValueAttribute    gda_data_model_ldap_get_attributes_at (GdaDataModel *model, gint col, gint row);
static GError             **gda_data_model_ldap_get_exceptions  (GdaDataModel *model);

static GObjectClass *parent_class = NULL;
#define CLASS(model) (GDA_DATA_MODEL_LDAP_CLASS (G_OBJECT_GET_CLASS (model)))

/*
 * Object init and dispose
 */
static void
gda_data_model_ldap_data_model_init (GdaDataModelIface *iface)
{
        iface->i_get_n_rows = gda_data_model_ldap_get_n_rows;
        iface->i_get_n_columns = gda_data_model_ldap_get_n_columns;
        iface->i_describe_column = gda_data_model_ldap_describe_column;
        iface->i_get_access_flags = gda_data_model_ldap_get_access_flags;
        iface->i_get_value_at = NULL;
        iface->i_get_attributes_at = gda_data_model_ldap_get_attributes_at;

        iface->i_create_iter = NULL;
        iface->i_iter_at_row = NULL;
        iface->i_iter_next = gda_data_model_ldap_iter_next;
        iface->i_iter_prev = NULL;

        iface->i_set_value_at = NULL;
	iface->i_iter_set_value = NULL;
        iface->i_set_values = NULL;
        iface->i_append_values = NULL;
        iface->i_append_row = NULL;
        iface->i_remove_row = NULL;
        iface->i_find_row = NULL;

        iface->i_set_notify = NULL;
        iface->i_get_notify = NULL;
        iface->i_send_hint = NULL;

	iface->i_get_exceptions = gda_data_model_ldap_get_exceptions;
}

static void
gda_data_model_ldap_init (GdaDataModelLdap *model,
			  G_GNUC_UNUSED GdaDataModelLdapClass *klass)
{
	GdaColumn *col;

	g_return_if_fail (GDA_IS_DATA_MODEL_LDAP (model));

	model->priv = g_new0 (GdaDataModelLdapPrivate, 1);
	model->priv->cnc = NULL;
	model->priv->base_dn = NULL;
	model->priv->use_rdn = FALSE;
	model->priv->filter = g_strdup ("(objectClass=*)");
	model->priv->iter_row = -1;
	model->priv->default_mv_action = MULTIPLE_VALUE_ACTION_SET_INVALID;
	model->priv->top_exec = NULL;
	model->priv->current_exec = NULL;
	model->priv->attributes = NULL;
	model->priv->truncated = FALSE;
	model->priv->exceptions = NULL;
	model->priv->row_mult = NULL;

	/* add the "dn" column */
	col = gda_column_new ();
	gda_column_set_name (col, "dn");
	gda_column_set_g_type (col, G_TYPE_STRING);
	gda_column_set_allow_null (col, FALSE);
	gda_column_set_description (col, _("Distinguished name"));
	model->priv->columns = g_list_prepend (NULL, col);
	model->priv->column_mv_actions = g_array_new (FALSE, FALSE, sizeof (MultipleValueAction));
	
	model->priv->n_columns = g_list_length (model->priv->columns);
	model->priv->scope = GDA_LDAP_SEARCH_BASE;
}

static void
gda_data_model_ldap_class_init (GdaDataModelLdapClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	/* properties */
        object_class->set_property = gda_data_model_ldap_set_property;
        object_class->get_property = gda_data_model_ldap_get_property;
        g_object_class_install_property (object_class, PROP_CNC,
                                         g_param_spec_object ("cnc", NULL, "LDAP connection",
							      GDA_TYPE_LDAP_CONNECTION,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class, PROP_BASE,
                                         g_param_spec_string ("base", NULL, "Base DN", NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_FILTER,
                                         g_param_spec_string ("filter", NULL, "LDAP filter", NULL,
                                                              G_PARAM_READABLE | G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
        g_object_class_install_property (object_class, PROP_ATTRIBUTES,
                                         g_param_spec_string ("attributes", NULL, "LDAP attributes", NULL,
                                                              G_PARAM_WRITABLE |
                                                              G_PARAM_CONSTRUCT_ONLY));
	g_object_class_install_property (object_class, PROP_SCOPE,
                                         g_param_spec_int ("scope", NULL, "LDAP search scope",
							   GDA_LDAP_SEARCH_BASE,
							   GDA_LDAP_SEARCH_SUBTREE,
							   GDA_LDAP_SEARCH_BASE,
							   G_PARAM_WRITABLE | G_PARAM_READABLE |
							   G_PARAM_CONSTRUCT_ONLY));

	g_object_class_install_property (object_class, PROP_USE_RDN,
                                         g_param_spec_boolean ("use-rdn", NULL, "Return Relative DN instead of complete DN",
							       FALSE, G_PARAM_WRITABLE | G_PARAM_READABLE));


	/* virtual functions */
	object_class->dispose = gda_data_model_ldap_dispose;
}

static void
gda_data_model_ldap_dispose (GObject * object)
{
	GdaDataModelLdap *model = (GdaDataModelLdap *) object;

	g_return_if_fail (GDA_IS_DATA_MODEL_LDAP (model));

	if (model->priv) {
		if (model->priv->row_mult)
			row_multiplier_free (model->priv->row_mult);
		if (model->priv->cnc)
			g_object_unref (model->priv->cnc);
		if (model->priv->columns) {
                        g_list_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
                        g_list_free (model->priv->columns);
                        model->priv->columns = NULL;
                }
		if (model->priv->attributes) {
			gint i;
			for (i = 0; i < model->priv->attributes->len; i++) {
				gchar *tmp;
				tmp = g_array_index (model->priv->attributes, gchar*, i);
				g_free (tmp);
			}
			g_array_free (model->priv->attributes, TRUE);
		}
		if (model->priv->column_mv_actions)
			g_array_free (model->priv->column_mv_actions, TRUE);

		if (model->priv->top_exec)
			ldap_part_free (model->priv->top_exec);

		g_free (model->priv->base_dn);
		g_free (model->priv->filter);

		if (model->priv->exceptions) {
			gint i; 
			for (i = 0; i < model->priv->exceptions->len; i++) {
				GError *e;
				e = g_array_index (model->priv->exceptions, GError*, i);
				g_error_free (e);
			}
			g_array_free (model->priv->exceptions, TRUE);
		}

		g_free (model->priv);
		model->priv = NULL;
	}

	parent_class->dispose (object);
}

GType
gdaprov_data_model_ldap_get_type (void)
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) {
		static GStaticMutex registering = G_STATIC_MUTEX_INIT;
		static const GTypeInfo info = {
			sizeof (GdaDataModelLdapClass),
			(GBaseInitFunc) NULL,
			(GBaseFinalizeFunc) NULL,
			(GClassInitFunc) gda_data_model_ldap_class_init,
			NULL,
			NULL,
			sizeof (GdaDataModelLdap),
			0,
			(GInstanceInitFunc) gda_data_model_ldap_init,
			0
		};
		static const GInterfaceInfo data_model_info = {
                        (GInterfaceInitFunc) gda_data_model_ldap_data_model_init,
                        NULL,
                        NULL
                };

		g_static_mutex_lock (&registering);
		if (type == 0) {
			type = g_type_register_static (G_TYPE_OBJECT, "GdaDataModelLdap", &info, 0);
			g_type_add_interface_static (type, GDA_TYPE_DATA_MODEL, &data_model_info);
		}
		g_static_mutex_unlock (&registering);
	}
	return type;
}

static void
gda_data_model_ldap_set_property (GObject *object,
				  guint param_id,
				  const GValue *value,
				  GParamSpec *pspec)
{
        GdaDataModelLdap *model;
        const gchar *string;

        model = GDA_DATA_MODEL_LDAP (object);
        if (model->priv) {
                switch (param_id) {
                case PROP_CNC: {
			GdaConnection *cnc;
			cnc = g_value_get_object (value);
			if (cnc) {
				if (g_object_get_data ((GObject*) cnc,
						       "__gda_connection_LDAP") != (gpointer) 0x01) {
					g_warning ("cnc is not an LDAP connection");
					break;
				}
				model->priv->cnc = g_object_ref (cnc);
			}
			break;
		}
                case PROP_BASE:
			string = g_value_get_string (value);
			if (string)
				model->priv->base_dn = g_strdup (string);
			break;
		case PROP_FILTER:
			string = g_value_get_string (value);
			if (string) {
				g_free (model->priv->filter);
				model->priv->filter = g_strdup (string);
			}
			break;
		case PROP_ATTRIBUTES: {
			const gchar *csv;
			csv = g_value_get_string (value);
			if (csv && *csv) {
				if (model->priv->columns) {
					g_list_foreach (model->priv->columns, (GFunc) g_object_unref, NULL);
					g_list_free (model->priv->columns);
				}
				if (model->priv->column_mv_actions) {
					g_array_free (model->priv->column_mv_actions, TRUE);
					model->priv->column_mv_actions = NULL;
				}

				model->priv->columns = _ldap_compute_columns (model->priv->cnc, csv,
									      &model->priv->attributes,
									      model->priv->default_mv_action,
									      &model->priv->column_mv_actions);
				if (model->priv->use_rdn)
					gda_column_set_description (GDA_COLUMN (model->priv->columns->data),
								    _("Relative distinguished name"));
				else
					gda_column_set_description (GDA_COLUMN (model->priv->columns->data),
								    _("Distinguished name"));
				model->priv->n_columns = g_list_length (model->priv->columns);
			}
			break;
		}
		case PROP_SCOPE:
			model->priv->scope = g_value_get_int (value);
			break;
		case PROP_USE_RDN:
			model->priv->use_rdn = g_value_get_boolean (value);
			if (model->priv->columns && model->priv->use_rdn)
				gda_column_set_description (GDA_COLUMN (model->priv->columns->data),
							    _("Relative distinguished name"));
			else
				gda_column_set_description (GDA_COLUMN (model->priv->columns->data),
							    _("Distinguished name"));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

static void
gda_data_model_ldap_get_property (GObject *object,
				  guint param_id,
				  GValue *value,
				  GParamSpec *pspec)
{
        GdaDataModelLdap *model;

        model = GDA_DATA_MODEL_LDAP (object);
        if (model->priv) {
                switch (param_id) {
                case PROP_CNC:
			g_value_set_object (value, model->priv->cnc);
			break;
                case PROP_BASE:
			g_value_set_string (value, model->priv->base_dn);
			break;
                case PROP_FILTER:
			g_value_set_string (value, model->priv->filter);
			break;
		case PROP_SCOPE:
			g_value_set_int (value, model->priv->scope);
			break;
		case PROP_USE_RDN:
		        g_value_set_boolean (value, model->priv->use_rdn);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
			break;
		}
	}
}

GList *
gdaprov_data_model_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes)
{
	return _ldap_compute_columns (cnc, attributes, NULL, MULTIPLE_VALUE_ACTION_SET_INVALID,
				      NULL);
}

/*
 * _ldap_compute_columns
 * @cnc: a #GdaConnection
 * @attributes: a string
 * @out_attrs_array: a place to store an array of strings (terminated by a %NULL), or %NULL
 * @default_mva: the default #MultipleValueAction if none speficied
 * @out_mv_actions: a place to store an array of MultipleValueAction, or %NULL
 *
 * Returns: (transfer full) (element-type GdaColumn): a list of #GdaColumn objects
 */
static GList *
_ldap_compute_columns (GdaConnection *cnc, const gchar *attributes,
		       GArray **out_attrs_array,
		       MultipleValueAction default_mva, GArray **out_mv_actions)
{
	gchar **array;
	gint i;
	GdaColumn *col;
	LdapConnectionData *cdata = NULL;
	GList *columns = NULL;
	GArray *attrs = NULL, *mva = NULL;
	GHashTable *colnames; /* key = column name, 0x01 */
	colnames = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	if (out_attrs_array) {
		attrs = g_array_new (TRUE, FALSE, sizeof (gchar*));
		*out_attrs_array = attrs;
	}
	if (out_mv_actions) {
		mva = g_array_new (FALSE, FALSE, sizeof (MultipleValueAction));
		*out_mv_actions = mva;
	}

	/* always add the DN column */
	col = gda_column_new ();
	gda_column_set_name (col, "dn");
	gda_column_set_g_type (col, G_TYPE_STRING);
	gda_column_set_allow_null (col, FALSE);
	columns = g_list_prepend (NULL, col);
	g_hash_table_insert (colnames, g_strdup ("dn"), (gpointer) 0x01);

	if (!attributes || !*attributes)
		return columns;

	/* parse input string */
	if (cnc) {
		g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
		cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
	}
	array = g_strsplit (attributes, ",", 0);
	for (i = 0; array [i]; i++) {
		GType coltype = GDA_TYPE_NULL;
		gchar **sub, *tmp;
		const gchar *mvaspec = NULL;
		MultipleValueAction act = default_mva;
		
		g_strstrip (array [i]);
		sub = g_strsplit (array[i], "::", 3);
		g_strstrip (sub [0]);
		if (sub [1]) {
			g_strstrip (sub [1]);
			if (sub [2]) {
				g_strstrip (sub [2]);
				mvaspec = sub [2];
			}
		}

		coltype = gda_ldap_get_g_type (cdata, sub [0], sub [1]);
		tmp = g_strdup (sub [0]);
		if (attrs)
			g_array_append_val (attrs, tmp);
		if (g_hash_table_lookup (colnames, sub [0])) {
			/* can't have twice the same LDAP attribute */
			g_strfreev (sub);
			continue;
		}
		col = gda_column_new ();
		gda_column_set_name (col, sub [0]);
		gda_column_set_description (col, sub [0]);
		g_hash_table_insert (colnames, g_strdup (sub [0]), (gpointer) 0x01);
		gda_column_set_g_type (col, coltype);
		gda_column_set_allow_null (col, TRUE);
		columns = g_list_prepend (columns, col);
		if (mva) {
			if (! mvaspec && sub [1] && (gda_g_type_from_string (sub [1]) == G_TYPE_INVALID))
				mvaspec = sub [1];
			if (mvaspec) {
				if ((*mvaspec == '0' && !mvaspec[1]) || !g_ascii_strcasecmp (mvaspec, "null"))
					act = MULTIPLE_VALUE_ACTION_SET_NULL;
				else if (!g_ascii_strcasecmp (mvaspec, "csv"))
					act = MULTIPLE_VALUE_ACTION_CSV_STRING;
				if ((*mvaspec == '*' && !mvaspec[1]) || !g_ascii_strncasecmp (mvaspec, "mult", 4))
					act = MULTIPLE_VALUE_ACTION_MULTIPLY;
				else if (!g_ascii_strcasecmp (mvaspec, "error"))
					act = MULTIPLE_VALUE_ACTION_SET_INVALID;
				else if (!strcmp (mvaspec, "1"))
					act = MULTIPLE_VALUE_ACTION_FIRST;
				else if (!g_ascii_strcasecmp (mvaspec, "concat"))
					act = MULTIPLE_VALUE_ACTION_CONCAT;
			}
			g_array_append_val (mva, act);
		}
		/*g_print ("Defined model column %s (type=>%s) (mva=>%d)\n", array[i],
		  gda_g_type_to_string (coltype), act);*/
		g_strfreev (sub);
	} 
	g_strfreev (array);
	g_hash_table_destroy (colnames);
	return g_list_reverse (columns);
}


/*
 * _gdaprov_data_model_ldap_new:
 * @cnc: an LDAP opened connection
 * @base_dn: the base DN to search on, or %NULL
 * @filter: an LDAP filter (for example "(objectClass=*)");
 * @attributes: the list of attributes to fetch, each in the format <attname>[::<GType>] (+CSV,...)
 * @scope: the search scope
 *
 * Creates a new #GdaDataModel object to extract some LDAP contents
 *
 * Returns: a new #GdaDataModel
 */
GdaDataModel *
_gdaprov_data_model_ldap_new (GdaConnection *cnc, const gchar *base_dn, const gchar *filter,
			      const gchar *attributes, GdaLdapSearchScope scope)
{
	g_return_val_if_fail (GDA_IS_CONNECTION (cnc), NULL);

	return (GdaDataModel *) g_object_new (GDA_TYPE_DATA_MODEL_LDAP, "cnc", cnc, 
					      "base", base_dn,
					      "filter", filter, "attributes", attributes,
					      "scope", scope,
					      NULL);
}

static gint
gda_data_model_ldap_get_n_rows (GdaDataModel *model)
{
	GdaDataModelLdap *imodel = (GdaDataModelLdap *) model;

	g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (imodel), -1);
	g_return_val_if_fail (imodel->priv != NULL, -1);

	return -1;
}

static gint
gda_data_model_ldap_get_n_columns (GdaDataModel *model)
{
	GdaDataModelLdap *imodel;
        g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), 0);
        imodel = GDA_DATA_MODEL_LDAP (model);
        g_return_val_if_fail (imodel->priv, 0);

        if (imodel->priv->columns)
                return imodel->priv->n_columns;
        else
                return 0;
}

static GdaColumn *
gda_data_model_ldap_describe_column (GdaDataModel *model, gint col)
{
	GdaDataModelLdap *imodel;
        g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), NULL);
        imodel = GDA_DATA_MODEL_LDAP (model);
        g_return_val_if_fail (imodel->priv, NULL);

        if (imodel->priv->columns)
                return g_list_nth_data (imodel->priv->columns, col);
        else
                return NULL;
}

static GdaDataModelAccessFlags
gda_data_model_ldap_get_access_flags (GdaDataModel *model)
{
	GdaDataModelLdap *imodel;
        GdaDataModelAccessFlags flags;

        g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), 0);
        imodel = GDA_DATA_MODEL_LDAP (model);
        g_return_val_if_fail (imodel->priv, 0);

	flags = GDA_DATA_MODEL_ACCESS_CURSOR_FORWARD;

        return flags;
}

static gchar *
csv_quote (const gchar *string)
{
	gchar *retval, *ptrd;
	const gchar *ptrs;
	retval = g_new (gchar, strlen (string) * 2 + 3);
	*retval = '"';
	for (ptrd = retval + 1, ptrs = string; *ptrs; ptrs++) {
		if (*ptrs == '"') {
			*ptrd = '"';
			ptrd++;
		}
		*ptrd = *ptrs;
		ptrd++;
	}
	*ptrd = '"';
	ptrd++;
	*ptrd = 0;
	return retval;
}

static void
update_iter_from_ldap_row (GdaDataModelLdap *imodel, GdaDataModelIter *iter)
{
	gboolean update_model;
	BerElement* ber;
	char *attr;
	GdaHolder *holder;
	gint j, nb;
	LdapConnectionData *cdata;
	GSList *holders_set = NULL;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (imodel->priv->cnc));
	g_return_if_fail (cdata);

	g_object_get (G_OBJECT (iter), "update-model", &update_model, NULL);
	g_object_set (G_OBJECT (iter), "update-model", FALSE, NULL);
		
	/* column 0 is the DN */
	holder = GDA_HOLDER (GDA_SET (iter)->holders->data);
	attr = ldap_get_dn (cdata->handle, imodel->priv->current_exec->ldap_row);
	if (attr) {
		gchar *userdn;
		if (gda_ldap_parse_dn (attr, &userdn)) {
			if (imodel->priv->base_dn && *imodel->priv->base_dn &&
			    imodel->priv->use_rdn &&
			    g_str_has_suffix (userdn, imodel->priv->base_dn)) {
				gint i;
				i = strlen (userdn) - strlen (imodel->priv->base_dn);
				if (i > 0) {
					userdn [i] = 0;
					for (i--; (i >= 0) && (userdn [i] != ','); i--);
					if ((i >= 0) && userdn [i] == ',')
						userdn [i] = 0;
				}
			}
			gda_holder_set_value_str (holder, NULL, userdn, NULL);
			g_free (userdn);
		}
		else
			gda_holder_force_invalid (holder);
		ldap_memfree (attr);
	}
	else
		gda_holder_force_invalid (holder);

	nb = g_slist_length (((GdaSet*) iter)->holders);
	for (j = 1; j < nb; j++) {
		holder = (GdaHolder*) (g_slist_nth_data (((GdaSet*) iter)->holders, j));
		gda_holder_set_value (holder, NULL, NULL);
	}

	if (imodel->priv->row_mult)
		goto out;

	for (attr = ldap_first_attribute (cdata->handle, imodel->priv->current_exec->ldap_row, &ber);
	     attr;
	     attr = ldap_next_attribute (cdata->handle, imodel->priv->current_exec->ldap_row, ber)) {
		BerValue **bvals;
		gboolean holder_added_to_cm = FALSE;

		holder = gda_set_get_holder ((GdaSet*) iter, attr);
		if (!holder)
			continue;

		j = g_slist_index (((GdaSet*) iter)->holders, holder);

		bvals = ldap_get_values_len (cdata->handle,
					     imodel->priv->current_exec->ldap_row, attr);
		if (bvals) {
			if (bvals[0] && bvals[1]) {
				/* multiple values */
				MultipleValueAction act;
				act = g_array_index (imodel->priv->column_mv_actions,
						     MultipleValueAction, j-1);
				switch (act) {
				case MULTIPLE_VALUE_ACTION_SET_NULL:
					gda_holder_set_value (holder, NULL, NULL);
					break;
				case MULTIPLE_VALUE_ACTION_CSV_STRING:
					if ((gda_holder_get_g_type (holder) == G_TYPE_STRING)) {
						GString *string = NULL;
						gint i;
						GValue *value;
						for (i = 0; bvals[i]; i++) {
							gchar *tmp;
							if (string)
								g_string_append_c (string, ',');
							else
								string = g_string_new ("");
							tmp = csv_quote (bvals[i]->bv_val);
							g_string_append (string, tmp);
							g_free (tmp);
						}
						value = gda_value_new (G_TYPE_STRING);
						g_value_take_string (value, string->str);
						g_string_free (string, FALSE);
						gda_holder_take_value (holder, value, NULL);
					}
					else
						gda_holder_force_invalid (holder);
					break;
				case MULTIPLE_VALUE_ACTION_MULTIPLY: {
					ColumnMultiplier *cm;
					if (! imodel->priv->row_mult) {
						imodel->priv->row_mult = row_multiplier_new ();
						/* create @cm for the previous columns */
						GSList *list;
						for (list = holders_set; list; list = list->next) {
							GdaHolder *ch;
							ch = (GdaHolder*) list->data;
							cm = column_multiplier_new (ch,
							               gda_holder_get_value (ch));
							g_array_append_val (imodel->priv->row_mult->cms, cm);
						}
					}
					/* add new @cm */
					cm = column_multiplier_new (holder, NULL);
					gint i;
					for (i = 0; bvals[i]; i++) {
						GValue *value;
						value = gda_ldap_attr_value_to_g_value (cdata,
											gda_holder_get_g_type (holder),
											bvals[i]);
						g_array_append_val (cm->values, value); /* value can be %NULL */
					}
					g_array_append_val (imodel->priv->row_mult->cms, cm);
					holder_added_to_cm = TRUE;
					break;
				}
				case MULTIPLE_VALUE_ACTION_FIRST:
					if ((gda_holder_get_g_type (holder) == G_TYPE_STRING)) {
						GValue *value;
						value = gda_value_new (G_TYPE_STRING);
						g_value_set_string (value, bvals[0]->bv_val);
						gda_holder_take_value (holder, value, NULL);
					}
					else
						gda_holder_force_invalid (holder);
					break;
				case MULTIPLE_VALUE_ACTION_CONCAT:
					if ((gda_holder_get_g_type (holder) == G_TYPE_STRING)) {
						GString *string = NULL;
						gint i;
						GValue *value;
						for (i = 0; bvals[i]; i++) {
							if (string)
								g_string_append_c (string, '\n');
							else
								string = g_string_new ("");
							g_string_append (string, bvals[i]->bv_val);
						}
						value = gda_value_new (G_TYPE_STRING);
						g_value_take_string (value, string->str);
						g_string_free (string, FALSE);
						gda_holder_take_value (holder, value, NULL);
					}
					else
						gda_holder_force_invalid (holder);
					break;
				case MULTIPLE_VALUE_ACTION_SET_INVALID:
				default:
					gda_holder_force_invalid (holder);
					break;
				}
			}
			else if (bvals[0]) {
				/* convert string to the correct type */
				GValue *value;
				value = gda_ldap_attr_value_to_g_value (cdata, gda_holder_get_g_type (holder),
									bvals[0]);
				if (value)
					gda_holder_take_value (holder, value, NULL);
				else
					gda_holder_force_invalid (holder);
			}
			else
				gda_holder_set_value (holder, NULL, NULL);
			ldap_value_free_len (bvals);
		}
		else
			gda_holder_set_value (holder, NULL, NULL);
		ldap_memfree (attr);
		holders_set = g_slist_prepend (holders_set, holder);
		if (imodel->priv->row_mult && !holder_added_to_cm) {
			ColumnMultiplier *cm;
			cm = column_multiplier_new (holder, gda_holder_get_value (holder));
			g_array_append_val (imodel->priv->row_mult->cms, cm);
		}
	}
	if (holders_set)
		g_slist_free (holders_set);

	if (ber)
		ber_free (ber, 0);

 out:
	if (imodel->priv->row_mult)
		row_multiplier_set_holders (imodel->priv->row_mult);

	if (gda_data_model_iter_is_valid (iter)) {
		imodel->priv->iter_row ++;
		if ((imodel->priv->iter_row == imodel->priv->n_rows - 1) && imodel->priv->truncated) {
			GError *e = NULL;
			g_set_error (&e, GDA_DATA_MODEL_ERROR,
				     GDA_DATA_MODEL_TRUNCATED_ERROR,
				     _("Truncated result because LDAP server limit encountered"));
			add_exception (imodel, e);
		}
	}
	else
		imodel->priv->iter_row = 0;

	g_object_set (G_OBJECT (iter), "current-row", imodel->priv->iter_row,
		      "update-model", update_model, NULL);
}

/*
 * Add an exception to @model
 * WARNING: steals @e
 */
static void
add_exception (GdaDataModelLdap *model, GError *e)
{
	if (!model->priv->exceptions)
		model->priv->exceptions = g_array_new (TRUE, FALSE, sizeof (GError*));
	g_array_append_val (model->priv->exceptions, e);
}

/*
 * Execute model->priv->current_exec and either:
 *     - sets model->priv->current_exec->ldap_msg, or
 *     - create some children and execute one, or
 *     - sets model->priv->current_exec->ldap_msg to %NULL if an error occurred
 *
 * In any case model->priv->current_exec->executed is set to %TRUE and should be %FALSE when entering
 * the function (ie. for any LdapConnectionData this function has to be called at most once)
 */
static void
execute_ldap_search (GdaDataModelLdap *model)
{
	LDAPMessage *msg = NULL;
	int lscope, res = 0;
	LdapConnectionData *cdata;
	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (model->priv->cnc));
	g_return_if_fail (cdata);

	g_assert (model->priv->current_exec);
	g_assert (! model->priv->current_exec->executed);

	switch (model->priv->current_exec->scope) {
	default:
	case GDA_LDAP_SEARCH_BASE:
		lscope = LDAP_SCOPE_BASE;
		break;
	case GDA_LDAP_SEARCH_ONELEVEL:
		lscope = LDAP_SCOPE_ONELEVEL;
		break;
	case GDA_LDAP_SEARCH_SUBTREE:
		lscope = LDAP_SCOPE_SUBTREE;
		break;
	}
	
#ifdef GDA_DEBUG_SUBSEARCHES
	if (model->priv->scope == GDA_LDAP_SEARCH_SUBTREE) {
		g_print ("Model %p model->priv->top_exec:\n", model);
		ldap_part_dump (model->priv->top_exec);
	}
#endif
	retry:
#ifdef GDA_DEBUG_SUBSEARCHES_FORCE
	/* force sub searches for 2 levels */
	static gint sims = 10;
	if ((sims > 0) &&
	    (model->priv->scope == GDA_LDAP_SEARCH_SUBTREE) &&
	    (! model->priv->current_exec->parent || ! model->priv->current_exec->parent->parent)) {
		g_print ("Simulating LDAP_ADMINLIMIT_EXCEEDED\n");
		res = LDAP_ADMINLIMIT_EXCEEDED;
		sims --;
	}
	if (res == 0)
#endif

	res = ldap_search_ext_s (cdata->handle, model->priv->current_exec->base_dn, lscope,
				 model->priv->filter,
				 (char**) model->priv->attributes->data, 0,
				 NULL, NULL, NULL, -1,
				 &msg);
	model->priv->current_exec->executed = TRUE;

	switch (res) {
	case LDAP_SUCCESS:
	case LDAP_NO_SUCH_OBJECT:
		/* all Ok */
		model->priv->current_exec->ldap_msg = msg;
		model->priv->current_exec->nb_entries = ldap_count_entries (cdata->handle, msg);
#ifdef GDA_DEBUG_SUBSEARCHES
		g_print ("model->priv->current_exec->nb_entries = %d\n",
			 model->priv->current_exec->nb_entries);
#endif
		break;

	case LDAP_ADMINLIMIT_EXCEEDED:
	case LDAP_SIZELIMIT_EXCEEDED:
	case LDAP_TIMELIMIT_EXCEEDED: {
#ifdef GDA_DEBUG_SUBSEARCHES
		g_print ("LIMIT_EXCEEDED!\n");
#endif
		gboolean handled = FALSE;
		if (model->priv->scope == GDA_LDAP_SEARCH_SUBTREE) {
			gboolean split_error;
			if (ldap_part_split (model->priv->current_exec, model, &split_error)) {
				/* create some children to re-run the search */
				if (msg)
					ldap_msgfree (msg);
				model->priv->current_exec = LDAP_PART (model->priv->current_exec->children->data);
				execute_ldap_search (model);
				handled = TRUE;
			}
			else if (!split_error) {
				LdapPart *next;
				next = ldap_part_next (model->priv->current_exec, FALSE);
				if (next) {
					model->priv->current_exec = next;
					execute_ldap_search (model);
					handled = TRUE;
				}
			}
		}
		if (!handled) {
			/* truncated output */
#ifdef GDA_DEBUG_SUBSEARCHES
			g_print ("Output truncated!\n");
#endif
			model->priv->truncated = TRUE;
			model->priv->current_exec->ldap_msg = msg;
			model->priv->current_exec->nb_entries = ldap_count_entries (cdata->handle, msg);
		}
		break;
	}
	case LDAP_SERVER_DOWN: {
		gint i;
		for (i = 0; i < 5; i++) {
			if (gda_ldap_silently_rebind (cdata))
				goto retry;
			g_usleep (G_USEC_PER_SEC * 2);
		}
	}
	default: {
		/* error */
		GError *e = NULL;
		int ldap_errno;
		ldap_get_option (cdata->handle, LDAP_OPT_ERROR_NUMBER, &ldap_errno);
		g_set_error (&e, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_OTHER_ERROR,
			     "%s", ldap_err2string(ldap_errno));
		add_exception (model, e);
		return;
	}
	}

	if (model->priv->truncated) {
		/* compute totel number of rows now that we know it */
		if (model->priv->top_exec->ldap_msg)
			model->priv->n_rows = model->priv->current_exec->nb_entries;
		else {
			LdapPart *iter;
			model->priv->n_rows = 0;
			for (iter = model->priv->top_exec; iter; iter = ldap_part_next (iter, TRUE))
				model->priv->n_rows += iter->nb_entries;
		}
	}
#ifdef GDA_DEBUG_NO
	gint tmpnb = 0;
	if (model->priv->top_exec->ldap_msg)
		tmpnb = model->priv->current_exec->nb_entries;
	else {
		LdapPart *iter;
		for (iter = model->priv->top_exec; iter; iter = ldap_part_next (iter, TRUE))
			tmpnb += iter->nb_entries;
	}
	g_print ("So far found %d\n", tmpnb);
#endif
}

static gboolean
gda_data_model_ldap_iter_next (GdaDataModel *model, GdaDataModelIter *iter)
{
	GdaDataModelLdap *imodel;
	LdapConnectionData *cdata;
	LdapPart *cpart;

        g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), FALSE);
        g_return_val_if_fail (GDA_IS_DATA_MODEL_ITER (iter), FALSE);
        imodel = GDA_DATA_MODEL_LDAP (model);
        g_return_val_if_fail (imodel->priv, FALSE);

	if (! imodel->priv->cnc) {
		/* error */
		gda_data_model_iter_invalidate_contents (iter);
		return FALSE;
	}

	cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (imodel->priv->cnc));
	if (!cdata) {
		/* error */
		gda_data_model_iter_invalidate_contents (iter);
		return FALSE;
	}

	/* initialize LDAP search if necessary */
	if (! imodel->priv->base_dn)
		imodel->priv->base_dn = g_strdup (cdata->base_dn);
	if (! imodel->priv->attributes)
		imodel->priv->attributes = g_array_new (TRUE, FALSE, sizeof (gchar*));
	if (! imodel->priv->top_exec) {
		imodel->priv->top_exec = ldap_part_new (NULL, imodel->priv->base_dn, imodel->priv->scope);
		imodel->priv->current_exec = imodel->priv->top_exec;
	}

	while (imodel->priv->current_exec) {
		cpart = imodel->priv->current_exec;
		if (! cpart->executed)
			execute_ldap_search (imodel);
		cpart = imodel->priv->current_exec;
		if (! cpart->ldap_msg) {
			/* error somewhere */
			gda_data_model_iter_invalidate_contents (iter);
			return FALSE;
		}

		if (! cpart->ldap_row)
			/* not yet on 1st row */
			cpart->ldap_row = ldap_first_entry (cdata->handle, cpart->ldap_msg);
		else {
			if (imodel->priv->row_mult) {
				if (! row_multiplier_index_next (imodel->priv->row_mult)) {
					row_multiplier_free (imodel->priv->row_mult);
					imodel->priv->row_mult = NULL;
				}
			}
			if (! imodel->priv->row_mult) {
				/* move to the next row */
				cpart->ldap_row = ldap_next_entry (cdata->handle, cpart->ldap_row);
			}
		}
		
		if (cpart->ldap_row) {
			update_iter_from_ldap_row (imodel, iter);
			break;
		}
		else
			/* nothing more for this part, switch to the next one */
			imodel->priv->current_exec = ldap_part_next (cpart, FALSE);
	}

	if (!imodel->priv->current_exec) {
		/* execution is over */
		gda_data_model_iter_invalidate_contents (iter);
                g_object_set (G_OBJECT (iter), "current-row", -1, NULL);
		if (imodel->priv->truncated) {
			GError *e = NULL;
			g_set_error (&e, GDA_DATA_MODEL_ERROR,
				     GDA_DATA_MODEL_TRUNCATED_ERROR,
				     _("Truncated result because LDAP server limit encountered"));
			add_exception (imodel, e);
		}
		g_signal_emit_by_name (iter, "end-of-data");
                return FALSE;
	}

	return TRUE;
}

static GdaValueAttribute
gda_data_model_ldap_get_attributes_at (GdaDataModel *model, gint col, G_GNUC_UNUSED gint row)
{
	GdaDataModelLdap *imodel;
	GdaValueAttribute flags;
	GdaColumn *column;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), 0);
        imodel = GDA_DATA_MODEL_LDAP (model);

	if ((col < 0) || (col > imodel->priv->n_columns)) {
		/* error */
		/*
		gchar *tmp;
		tmp = g_strdup_printf (_("Column %d out of range (0-%d)"), col, imodel->priv->n_columns - 1);
		add_error (imodel, tmp);
		g_free (tmp);
		*/
		return 0;
	}

	flags = GDA_VALUE_ATTR_NO_MODIF;
	column = g_list_nth_data (imodel->priv->columns, col);
	if (gda_column_get_allow_null (column))
		flags |= GDA_VALUE_ATTR_CAN_BE_NULL;

	return flags;
}

static GError **
gda_data_model_ldap_get_exceptions (GdaDataModel *model)
{
	GdaDataModelLdap *imodel;
	g_return_val_if_fail (GDA_IS_DATA_MODEL_LDAP (model), NULL);
	imodel = GDA_DATA_MODEL_LDAP (model);
	if (imodel->priv->exceptions)
		return (GError**) imodel->priv->exceptions->data;
	else
		return NULL;
}

/*
 * Parts manipulations
 */
static LdapPart *
ldap_part_new (LdapPart *parent, const gchar *base_dn, GdaLdapSearchScope scope)
{
	LdapPart *part;
	if (!base_dn || !*base_dn)
		return NULL;
	part = g_new0 (LdapPart, 1);
	part->base_dn = g_strdup (base_dn);
	part->scope = scope;
	part->ldap_msg = NULL;
	part->ldap_row = NULL;
	part->children = NULL;
	part->parent = parent;
	return part;
}

static void
ldap_part_free (LdapPart *part)
{
	g_assert (part);
	g_free (part->base_dn);
	if (part->children) {
		g_slist_foreach (part->children, (GFunc) ldap_part_free, NULL);
		g_slist_free (part->children);
	}
	if (part->ldap_msg)
		ldap_msgfree (part->ldap_msg);
	g_free (part);
}

/*
 * Go to the next part in the tree
 */
static LdapPart *
ldap_part_next (LdapPart *part, gboolean executed)
{
	LdapPart *parent, *retval = NULL;
	if (part->children)
		retval = LDAP_PART (part->children->data);
	else {
		LdapPart *tmp;
		tmp = part;
		for (parent = tmp->parent; parent; parent = tmp->parent) {
			gint i;
			i = g_slist_index (parent->children, tmp);
			tmp = g_slist_nth_data (parent->children, i+1);
			if (tmp) {
				retval = tmp;
				break;
			}
			else
				tmp = parent;
		}
	}
	
	if (retval) {
		if (executed && !retval->executed)
			return ldap_part_next (retval, executed);
		else if (!executed && retval->executed)
			return ldap_part_next (retval, executed);
	}

	if (retval == part) {
		TO_IMPLEMENT;
	}
	g_assert (retval != part);
	return retval;
}

/*
 * Creates sub parts of @part, one for each immediate child pf @part->base_dn
 *
 * Returns: %TRUE if part->children is not %NULL
 */
static gboolean
ldap_part_split (LdapPart *part, GdaDataModelLdap *model, gboolean *out_error)
{
	/* fetch all children entries of @part, and build a child part
	 * for each of them */
	GdaDataModel *mparts;
	GdaDataModelIter *iter;
	
	if (out_error)
		*out_error = FALSE;
	g_assert (!part->children);
	mparts = _gdaprov_data_model_ldap_new (model->priv->cnc, part->base_dn, NULL, NULL,
					      GDA_LDAP_SEARCH_ONELEVEL);
	if (!mparts) {
		if (out_error)
			*out_error = TRUE;
		return FALSE;
	}

	if (part->scope == GDA_LDAP_SEARCH_SUBTREE) {
		/* create a part for the node itself */
		LdapPart *sub = NULL;
		sub = ldap_part_new (part, part->base_dn, GDA_LDAP_SEARCH_BASE);
		g_assert (sub);
		part->children = g_slist_prepend (part->children, sub);
	}

        iter = gda_data_model_create_iter (mparts);
        for (; gda_data_model_iter_move_next (iter); ) {
		const GValue *cvalue;
		LdapPart *sub = NULL;
		cvalue = gda_data_model_iter_get_value_at (iter, 0);
		if (cvalue) {
			gchar *tmp;
			tmp = gda_value_stringify (cvalue);
			sub = ldap_part_new (part, tmp, part->scope);
			if (sub)
				part->children = g_slist_prepend (part->children, sub);
                        g_free (tmp);
		}
		if (!sub) {
			/* error */
			g_slist_foreach (part->children, (GFunc) ldap_part_free, NULL);
			g_slist_free (part->children);
			part->children = NULL;
			break;
                }
        }
	g_object_unref (mparts);

	return part->children ? TRUE : FALSE;
}

#ifdef GDA_DEBUG_SUBSEARCHES
static void
ldap_part_dump_to_string (LdapPart *part, GString *string, gint index, gboolean recurs)
{
	gchar *indent;
	indent = g_new (gchar, index * 4 + 1);
	memset (indent, ' ', sizeof (gchar) * index * 4);
	indent [index * 4] = 0;
	g_string_append (string, indent);
	g_free (indent);

	g_string_append_printf (string, "part[%p] scope[%d] base[%s] %s nb_entries[%d]\n", part,
				part->scope, part->base_dn,
				part->executed ? "EXEC" : "non exec", part->nb_entries);
	
	if (recurs && part->children) {
		GSList *list;
		for (list = part->children; list; list = list->next)
			ldap_part_dump_to_string (LDAP_PART (list->data), string, index+1, recurs);
	}
}

static void
ldap_part_dump (LdapPart *part)
{
	GString *string;
	string = g_string_new ("");
	ldap_part_dump_to_string (part, string, 0, TRUE);
	g_print ("%s\n", string->str);
	g_string_free (string, TRUE);
}
#endif


/*
 * Row & column multiplier
 */

static ColumnMultiplier *
column_multiplier_new (GdaHolder *holder, const GValue *value)
{
	ColumnMultiplier *cm;
	cm = g_new0 (ColumnMultiplier, 1);
	cm->holder = g_object_ref (holder);
	cm->index = 0;
	cm->values = g_array_new (FALSE, FALSE, sizeof (GValue*));
	if (value) {
		GValue *copy;
		copy = gda_value_copy (value);
		g_array_append_val (cm->values, copy);
	}
	return cm;
}

static RowMultiplier *
row_multiplier_new (void)
{
	RowMultiplier *rm;
	rm = g_new0 (RowMultiplier, 1);
	rm->cms = g_array_new (FALSE, FALSE, sizeof (ColumnMultiplier*));
	return rm;
}

static void
row_multiplier_set_holders (RowMultiplier *rm)
{
	gint i;
	for (i = 0; i < rm->cms->len; i++) {
		ColumnMultiplier *cm;
		GValue *value;
		cm = g_array_index (rm->cms, ColumnMultiplier*, i);
		value = g_array_index (cm->values, GValue *, cm->index);
		if (value)
			gda_holder_set_value (cm->holder, value, NULL);
		else
			gda_holder_force_invalid (cm->holder);
	}
}

static void
row_multiplier_free (RowMultiplier *rm)
{
	gint i;
	for (i = 0; i < rm->cms->len; i++) {
		ColumnMultiplier *cm;
		cm = g_array_index (rm->cms, ColumnMultiplier*, i);
		gint j;
		for (j = 0; j < cm->values->len; j++) {
			GValue *value;
			value = g_array_index (cm->values, GValue *, j);
			if (value)
				gda_value_free (value);
		}
		g_array_free (cm->values, TRUE);
		g_object_unref (cm->holder);
		g_free (cm);
	}
	g_array_free (rm->cms, TRUE);
	g_free (rm);
}

/*
 * move indexes one step forward
 */
static gboolean
row_multiplier_index_next (RowMultiplier *rm)
{
	gint i;
#ifdef GDA_DEBUG_NO
	g_print ("RM %p before:\n", rm);
	for (i = 0; i < rm->cms->len; i++) {
		ColumnMultiplier *cm;
		cm = g_array_index (rm->cms, ColumnMultiplier*, i);
		g_print (" %d ", cm->index);
	}
	g_print ("\n");
#endif

	for (i = 0; ; i++) {
		ColumnMultiplier *cm;
		if (i == rm->cms->len) {
#ifdef GDA_DEBUG_NO
			g_print ("RM %p [FALSE]:\n", rm);
			for (i = 0; i < rm->cms->len; i++) {
				ColumnMultiplier *cm;
				cm = g_array_index (rm->cms, ColumnMultiplier*, i);
				g_print (" %d ", cm->index);
			}
			g_print ("\n");
#endif
			return FALSE;
		}

		cm = g_array_index (rm->cms, ColumnMultiplier*, i);
		if (cm->index < (cm->values->len - 1)) {
			cm->index ++;
#ifdef GDA_DEBUG_NO
			g_print ("RM %p [TRUE]:\n", rm);
			for (i = 0; i < rm->cms->len; i++) {
				ColumnMultiplier *cm;
				cm = g_array_index (rm->cms, ColumnMultiplier*, i);
				g_print (" %d ", cm->index);
			}
			g_print ("\n");
#endif
			return TRUE;
		}
		else {
			gint j;
			for (j = 0; j < i; j++) {
				cm = g_array_index (rm->cms, ColumnMultiplier*, j);
				cm->index = 0;
			}
		}
	}
}
