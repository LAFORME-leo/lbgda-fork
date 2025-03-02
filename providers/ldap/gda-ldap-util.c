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

#include <glib-object.h>
#include <libgda/gda-value.h>
#include <glib/gi18n-lib.h>
#include "gda-ldap.h"
#include "gda-ldap-util.h"
#include <sqlite/virtual/gda-ldap-connection.h>
#include <gda-util.h>

static void
ldap_attribute_free (LdapAttribute *lat)
{
	g_free (lat->name);
	g_free (lat);
}

static void
ldap_class_free (GdaLdapClass *lcl)
{
	g_free (lcl->oid);
	g_strfreev (lcl->names);
	g_free (lcl->description);
	
	if (lcl->req_attributes)
		g_strfreev (lcl->req_attributes);

	if (lcl->opt_attributes)
		g_strfreev (lcl->opt_attributes);
	g_slist_free (lcl->parents);
	g_slist_free (lcl->children);
	g_free (lcl);
}

/*
 * Data copied from GQ's sources and transformed,
 * see ftp://ftp.rfc-editor.org/in-notes/rfc2252.txt
 */
static LdapAttrType ldap_types [] = {
	{ "1.3.6.1.4.1.1466.115.121.1.1",
	  "ACI Item",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.2",
	  "Access Point",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.3",
	  "Attribute Type Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.4",
	  "Audio",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.5",
	  "Binary",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.6",
	  "Bit String",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.7",
	  "Boolean",
	  G_TYPE_BOOLEAN
	},
	{ "1.3.6.1.4.1.1466.115.121.1.8",
	  "Certificate",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.9",
	  "Certificate List",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.10",
	  "Certificate Pair",
	  -1 /*GDA_TYPE_BINARY*/,
	},
	{ "1.3.6.1.4.1.1466.115.121.1.11",
	  "Country String",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.12",
	  "DN",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.13",
	  "Data Quality Syntax",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.14",
	  "Delivery Method",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.15",
	  "Directory String",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.16",
	  "DIT Content Rule Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.17",
	  "DIT Structure Rule Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.18",
	  "DL Submit Permission",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.19",
	  "DSA Quality Syntax",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.20",
	  "DSE Type",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.21",
	  "Enhanced Guide",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.22",
	  "Facsimile Telephone Number",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.23",
	  "Fax",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.24",
	  "Generalized Time",
	  -4 /*GDA_TYPE_TIMESTAMP*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.25",
	  "Guide",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.26",
	  "IA5 String",
	  G_TYPE_STRING /* as ASCII */
	},
	{ "1.3.6.1.4.1.1466.115.121.1.27",
	  "INTEGER",
	  G_TYPE_INT
	},
	{ "1.3.6.1.4.1.1466.115.121.1.28",
	  "JPEG",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.29",
	  "Master And Shadow Access Points",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.30",
	  "Matching Rule Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.31",
	  "Matching Rule Use Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.32",
	  "Mail Preference",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.33",
	  "MHS OR Address",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.34",
	  "Name And Optional UID",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.35",
	  "Name Form Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.36",
	  "Numeric String",
	  -3 /*GDA_TYPE_NUMERIC*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.37",
	  "Object Class Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.38",
	  "OID",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.39",
	  "Other Mailbox",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.40",
	  "Octet String",
	  -1 /*GDA_TYPE_BINARY*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.41",
	  "Postal Address",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.42",
	  "Protocol Information",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.43",
	  "Presentation Address",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.44",
	  "Printable String",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.45",
	  "Subtree Specification",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.46",
	  "Supplier Information",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.47",
	  "Supplier Or Consumer",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.48",
	  "Supplier And Consumer",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.49",
	  "Supported Algorithm",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.50",
	  "Telephone Number",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.51",
	  "Teletex Terminal Identifier",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.52",
	  "Telex Number",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.53",
	  "UTC Time",
	  -2 /*GDA_TYPE_TIME*/
	},
	{ "1.3.6.1.4.1.1466.115.121.1.54",
	  "LDAP Syntax Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.55",
	  "Modify Rights",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.56",
	  "LDAP Schema Definition",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.57",
	  "LDAP Schema Description",
	  G_TYPE_STRING
	},
	{ "1.3.6.1.4.1.1466.115.121.1.58",
	  "Substring Assertion",
	  G_TYPE_STRING
	}
};

LdapAttrType unknown_type = {
	"", "Unknown type", G_TYPE_STRING
};

/**
 * gda_ldap_get_type_info:
 *
 * Returns: the #LdapAttrType associated to @oid, NEVER NULL
 */
LdapAttrType *
gda_ldap_get_type_info (const gchar *oid)
{
	static GHashTable *hash = NULL;
	LdapAttrType *retval = NULL;
	if (hash) {
		if (oid)
			retval = g_hash_table_lookup (hash, oid);
	}
	else {
		hash = g_hash_table_new (g_str_hash, g_str_equal);
		gint i, nb;
		nb = sizeof (ldap_types) / sizeof (LdapAttrType);
		for (i = 0; i < nb; i++) {
			LdapAttrType *type;
			type = & (ldap_types[i]);
			if (type->gtype == -1)
				type->gtype = GDA_TYPE_BINARY;
			else if (type->gtype == -2)
				type->gtype = GDA_TYPE_TIME;
			else if (type->gtype == -3)
				type->gtype = GDA_TYPE_NUMERIC;
			else if (type->gtype == -4)
				type->gtype = GDA_TYPE_TIMESTAMP;
			g_hash_table_insert (hash, type->oid, type);
		}
		if (oid)
			retval = g_hash_table_lookup (hash, oid);
	}
	return retval ? retval : &unknown_type;
}

/**
 * gda_ldap_get_attr_info:
 *
 * Returns: the #LdapAttribute for @attribute, or %NULL
 */
LdapAttribute *
gda_ldap_get_attr_info (LdapConnectionData *cdata, const gchar *attribute)
{
	LdapAttribute *retval = NULL;
	if (! attribute || !cdata)
		return NULL;

	if (cdata->attributes_hash)
		return g_hash_table_lookup (cdata->attributes_hash, attribute);

	/* initialize known types */
	cdata->attributes_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
							NULL,
							(GDestroyNotify) ldap_attribute_free);
	

	if (cdata->attributes_cache_file) {
		/* try to load from cache file, which must contain one line per attribute:
		 * <syntax oid>,0|1,<attribute name>
		 */
		gchar *data;
		if (g_file_get_contents (cdata->attributes_cache_file, &data, NULL, NULL)) {
			gchar *start, *ptr;
			gchar **array;
			start = data;
			while (1) {
				gboolean done = FALSE;
				for (ptr = start; *ptr && (*ptr != '\n'); ptr++);
				if (*ptr == '\n')
					*ptr = 0;
				else
					done = TRUE;
				
				if (*start && (*start != '#')) {
					array = g_strsplit (start, ",", 3);
					if (array[0] && array[1] && array[2]) {
						LdapAttribute *lat;
						lat = g_new (LdapAttribute, 1);
						lat->name = g_strdup (array[2]);
						lat->type = gda_ldap_get_type_info (array[0]);
						lat->single_value = (*array[1] == '0' ? FALSE : TRUE);
						g_hash_table_insert (cdata->attributes_hash,
								     lat->name, lat);
						/*g_print ("CACHE ADDED [%s][%p][%d] for OID %s\n",
						  lat->name, lat->type, lat->single_value,
						  array[0]);*/
					}
					g_strfreev (array);
				}
				if (done)
					break;
				else
					start = ptr+1;
			}
			g_free (data);
			return g_hash_table_lookup (cdata->attributes_hash, attribute);
		}
	}

	GString *string = NULL;
	LDAPMessage *msg, *entry;
	int res;
	gchar *subschema = NULL;

	char *subschemasubentry[] = {"subschemaSubentry", NULL};
	char *schema_attrs[] = {"attributeTypes", NULL};
	
	/* look for subschema */
	res = ldap_search_ext_s (cdata->handle, "", LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 subschemasubentry, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	if (res != LDAP_SUCCESS)
		return NULL;

	if ((entry = ldap_first_entry (cdata->handle, msg))) {
		char *attr;
		BerElement *ber;
		if ((attr = ldap_first_attribute (cdata->handle, entry, &ber))) {
			BerValue **bvals;
			if ((bvals = ldap_get_values_len (cdata->handle, entry, attr))) {
				subschema = g_strdup (bvals[0]->bv_val);
				ldap_value_free_len (bvals);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (! subschema)
		return NULL;

	/* look for attributeTypes */
	res = ldap_search_ext_s (cdata->handle, subschema, LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 schema_attrs, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	g_free (subschema);
	if (res != LDAP_SUCCESS)
		return NULL;

	if (cdata->attributes_cache_file)
		string = g_string_new ("# Cache file. This file can safely be removed, in this case\n"
				       "# it will be automatically recreated.\n"
				       "# DO NOT MODIFY\n");
	for (entry = ldap_first_entry (cdata->handle, msg);
	     entry;
	     entry = ldap_next_entry (cdata->handle, msg)) {
		char *attr;
		BerElement *ber;
		for (attr = ldap_first_attribute (cdata->handle, msg, &ber);
		     attr;
		     attr = ldap_next_attribute (cdata->handle, msg, ber)) {
			if (strcasecmp(attr, "attributeTypes")) {
				ldap_memfree (attr);
				continue;
			}

			BerValue **bvals;
			bvals = ldap_get_values_len (cdata->handle, entry, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals[i]; i++) {
					LDAPAttributeType *at;
					const char *errp;
					int retcode;
					at = ldap_str2attributetype (bvals[i]->bv_val, &retcode,
								     &errp,
								     LDAP_SCHEMA_ALLOW_ALL);
					if (at && at->at_names && at->at_syntax_oid &&
					    at->at_names[0] && *(at->at_names[0])) {
						LdapAttribute *lat;
						lat = g_new (LdapAttribute, 1);
						lat->name = g_strdup (at->at_names [0]);
						lat->type = gda_ldap_get_type_info (at->at_syntax_oid);
						lat->single_value = (at->at_single_value == 0 ? FALSE : TRUE);
						g_hash_table_insert (cdata->attributes_hash,
								     lat->name, lat);
						/*g_print ("ADDED [%s][%p][%d] for OID %s\n",
						  lat->name, lat->type, lat->single_value,
						  at->at_syntax_oid);*/
						if (string)
							g_string_append_printf (string, "%s,%d,%s\n",
										at->at_syntax_oid,
										lat->single_value,
										lat->name);
									  
					}
					if (at)
						ldap_memfree (at);
				}
				ldap_value_free_len (bvals);
			}
			  
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (string) {
		if (! g_file_set_contents (cdata->attributes_cache_file, string->str, -1, NULL)) {
			gchar *dirname;
			dirname = g_path_get_dirname (cdata->attributes_cache_file);
			g_mkdir_with_parents (dirname, 0700);
			g_free (dirname);
			g_file_set_contents (cdata->attributes_cache_file, string->str, -1, NULL);
		}
		g_string_free (string, TRUE);
	}

	retval = g_hash_table_lookup (cdata->attributes_hash, attribute);
	return retval;
}

/*
 * Classes
 */
static gchar **make_array_from_strv (char **values, guint *out_size);
static void classes_h_func (GdaLdapClass *lcl, gchar **supclasses, LdapConnectionData *cdata);
static gint classes_sort (GdaLdapClass *lcl1, GdaLdapClass *lcl2);

/**
 * gdaprov_ldap_get_class_info:
 * @cnc: a #GdaLdapConnection (not %NULL)
 * @classname: the class name (not %NULL)
 *
 * Returns: the #GdaLdapClass for @classname, or %NULL
 */
GdaLdapClass *
gdaprov_ldap_get_class_info (GdaLdapConnection *cnc, const gchar *classname)
{
	GdaLdapClass *retval = NULL;
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (classname, NULL);
	
        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
		return NULL;

	if (cdata->classes_hash)
		return g_hash_table_lookup (cdata->classes_hash, classname);

	/* initialize known classes */
	cdata->classes_hash = g_hash_table_new_full (g_str_hash, g_str_equal,
						     NULL,
						     (GDestroyNotify) ldap_class_free);

	LDAPMessage *msg, *entry;
	int res;
	gchar *subschema = NULL;

	char *subschemasubentry[] = {"subschemaSubentry", NULL};
	char *schema_attrs[] = {"objectClasses", NULL};
	
	/* look for subschema */
	res = ldap_search_ext_s (cdata->handle, "", LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 subschemasubentry, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	if (res != LDAP_SUCCESS)
		return NULL;

	if ((entry = ldap_first_entry (cdata->handle, msg))) {
		char *attr;
		BerElement *ber;
		if ((attr = ldap_first_attribute (cdata->handle, entry, &ber))) {
			BerValue **bvals;
			if ((bvals = ldap_get_values_len (cdata->handle, entry, attr))) {
				subschema = g_strdup (bvals[0]->bv_val);
				ldap_value_free_len (bvals);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	if (! subschema)
		return NULL;

	/* look for attributeTypes */
	res = ldap_search_ext_s (cdata->handle, subschema, LDAP_SCOPE_BASE,
				 "(objectclass=*)",
				 schema_attrs, 0,
				 NULL, NULL, NULL, 0,
				 &msg);
	g_free (subschema);
	if (res != LDAP_SUCCESS)
		return NULL;

	GHashTable *h_refs;
	h_refs = g_hash_table_new_full (NULL, NULL, NULL, (GDestroyNotify) g_strfreev);
	for (entry = ldap_first_entry (cdata->handle, msg);
	     entry;
	     entry = ldap_next_entry (cdata->handle, msg)) {
		char *attr;
		BerElement *ber;
		for (attr = ldap_first_attribute (cdata->handle, msg, &ber);
		     attr;
		     attr = ldap_next_attribute (cdata->handle, msg, ber)) {
			if (strcasecmp(attr, "objectClasses")) {
				ldap_memfree (attr);
				continue;
			}

			BerValue **bvals;
			bvals = ldap_get_values_len (cdata->handle, entry, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals[i]; i++) {
					LDAPObjectClass *oc;
					const char *errp;
					int retcode;
					oc = ldap_str2objectclass (bvals[i]->bv_val, &retcode,
								   &errp,
								   LDAP_SCHEMA_ALLOW_ALL);
					if (oc && oc->oc_oid && oc->oc_names && oc->oc_names[0]) {
						GdaLdapClass *lcl;
						guint k;
						lcl = g_new0 (GdaLdapClass, 1);
						lcl->oid = g_strdup (oc->oc_oid);
//#define CLASS_DEBUG
#ifdef CLASS_DEBUG
						g_print ("FOUND CLASS\n");
#endif
						lcl->names = make_array_from_strv (oc->oc_names,
										   &(lcl->nb_names));
						for (k = 0; lcl->names[k]; k++) {
#ifdef CLASS_DEBUG
							g_print ("  oc_names[%d] = %s\n",
								 k, lcl->names[k]);
#endif
							g_hash_table_insert (cdata->classes_hash,
									     lcl->names[k],
									     lcl);
						}
						if (oc->oc_desc) {
#ifdef CLASS_DEBUG
							g_print ("  oc_desc = %s\n", oc->oc_desc);
#endif
							lcl->description = g_strdup (oc->oc_desc);
						}
#ifdef CLASS_DEBUG
						g_print ("  oc_kind = %d\n", oc->oc_kind);
#endif
						switch (oc->oc_kind) {
						case 0:
							lcl->kind = GDA_LDAP_CLASS_KIND_ABSTRACT;
							break;
						case 1:
							lcl->kind = GDA_LDAP_CLASS_KIND_STRUTURAL;
							break;
						case 2:
							lcl->kind = GDA_LDAP_CLASS_KIND_AUXILIARY;
							break;
						default:
							lcl->kind = GDA_LDAP_CLASS_KIND_UNKNOWN;
							break;
						}
						lcl->obsolete = oc->oc_obsolete;
#ifdef CLASS_DEBUG
						g_print ("  oc_obsolete = %d\n", oc->oc_obsolete);

#endif
						gchar **refs;
						refs = make_array_from_strv (oc->oc_sup_oids, NULL);
						if (refs)
							g_hash_table_insert (h_refs, lcl, refs);
						else
							cdata->top_classes = g_slist_insert_sorted (cdata->top_classes,
									     lcl, (GCompareFunc) classes_sort);
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_sup_oids && oc->oc_sup_oids[k]; k++)
							g_print ("  oc_sup_oids[0] = %s\n",
								 oc->oc_sup_oids[k]);
#endif

						lcl->req_attributes =
							make_array_from_strv (oc->oc_at_oids_must,
									      &(lcl->nb_req_attributes));
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_at_oids_must && oc->oc_at_oids_must[k]; k++)
							g_print ("  oc_at_oids_must[0] = %s\n",
								 oc->oc_at_oids_must[k]);
#endif
						lcl->opt_attributes =
							make_array_from_strv (oc->oc_at_oids_may,
									      &(lcl->nb_opt_attributes));
#ifdef CLASS_DEBUG
						for (k = 0; oc->oc_at_oids_may && oc->oc_at_oids_may[k]; k++)
							g_print ("  oc_at_oids_may[0] = %s\n",
								 oc->oc_at_oids_may[k]);
#endif
						  
					}
					if (oc)
						ldap_memfree (oc);
				}
				ldap_value_free_len (bvals);
			}
			  
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
	}
	ldap_msgfree (msg);

	/* create hierarchy */
	g_hash_table_foreach (h_refs, (GHFunc) classes_h_func, cdata);
	g_hash_table_destroy (h_refs);

	retval = g_hash_table_lookup (cdata->classes_hash, classname);
	return retval;
}

static gint
my_sort_func (gconstpointer a, gconstpointer b)
{
	gchar *sa, *sb;
	sa = * (gchar**) a;
	sb = * (gchar**) b;
	if (sa && sb)
		return g_utf8_collate (sa, sb);
	else if (sa)
		return -1;
	else if (sb)
		return 1;
	else
		return 0;
}

static gchar **
make_array_from_strv (char **values, guint *out_size)
{
	if (out_size)
		*out_size = 0;
	if (!values)
		return NULL;
	GArray *array;
	gint i;
	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	for (i = 0; values[i]; i++) {
		gchar *tmp;
		tmp = g_strdup (values [i]);
		g_array_append_val (array, tmp);
	}
	if (out_size)
		*out_size = array->len;

	g_array_sort (array, (GCompareFunc) my_sort_func);

	return (gchar**) g_array_free (array, FALSE);
}

static gint
classes_sort (GdaLdapClass *lcl1, GdaLdapClass *lcl2)
{
	return g_utf8_collate (lcl1->names[0], lcl2->names[0]);
}

static void
classes_h_func (GdaLdapClass *lcl, gchar **supclasses, LdapConnectionData *cdata)
{
	gint i;
	for (i = 0; supclasses [i]; i++) {
		GdaLdapClass *parent;
		gchar *clname = supclasses [i];
#ifdef CLASS_DEBUG
		g_print ("class [%s] inherits [%s]\n", lcl->names[0], clname);
#endif
		parent = g_hash_table_lookup (cdata->classes_hash, clname);
		if (!parent)
			continue;
		lcl->parents = g_slist_insert_sorted (lcl->parents, parent, (GCompareFunc) classes_sort);
		parent->children = g_slist_insert_sorted (parent->children, lcl, (GCompareFunc) classes_sort);
	}
	if ((i == 0) && !g_slist_find (cdata->top_classes, lcl))
		cdata->top_classes = g_slist_insert_sorted (cdata->top_classes, lcl, (GCompareFunc) classes_sort);
}

/*
 * _gda_ldap_get_top_classes
 */
const GSList *
gdaprov_ldap_get_top_classes (GdaLdapConnection *cnc)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	
        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
		return NULL;
	if (! cdata->classes_hash) {
		/* force classes init */
		gdaprov_ldap_get_class_info (cnc, "top");
	}
	return cdata->top_classes;
}

/*
 * gda_ldap_get_g_type
 *
 * Compute the GType either from a specified GType of from the attribute name (giving precedence
 * to the specified GType if any)
 */
GType
gda_ldap_get_g_type (LdapConnectionData *cdata, const gchar *attribute_name, const gchar *specified_gtype)
{
	GType coltype = GDA_TYPE_NULL;
	if (specified_gtype)
		coltype = gda_g_type_from_string (specified_gtype);
	if (coltype == GDA_TYPE_NULL) {
		LdapAttribute *lat;
		lat = gda_ldap_get_attr_info (cdata, attribute_name);
		if (lat)
			coltype = lat->type->gtype;
	}
	if (coltype == GDA_TYPE_NULL)
		coltype = G_TYPE_STRING;
	return coltype;
}

/*
 * gda_ldap_attr_value_to_g_value:
 * Converts a #BerValue to a new #GValue
 *
 * Returns: a new #GValue, or %NULL on error
 */
GValue *
gda_ldap_attr_value_to_g_value (LdapConnectionData *cdata, GType type, BerValue *bv)
{
	GValue *value = NULL;
	if ((type == GDA_TYPE_TIMESTAMP) ||
	    (type == G_TYPE_DATE)) {
		/* see ftp://ftp.rfc-editor.org/in-notes/rfc4517.txt,
		 * section 3.3.13: Generalized Time
		 */
		GTimeVal tv;
		gboolean conv;
		if (! (conv = g_time_val_from_iso8601 (bv->bv_val,
						       &tv))) {
			/* Add the 'T' char */
			gchar *tmp, *str;
			gint i, len;
			str = bv->bv_val;
			len = strlen (str);
			if (len > 8) {
				tmp = g_new (gchar, len + 2);
				for (i = 0; i < 8; i++)
					tmp[i] = str[i];
				tmp [8] = 'T';
				for (i = 9; str[i]; i++)
					tmp[i] = str[i-1];
				tmp[i] = 0;
				conv = g_time_val_from_iso8601 (tmp, &tv);
				g_free (tmp);
			}
		}
		if (conv) {
			struct tm *ptm;
#ifdef HAVE_LOCALTIME_R
			struct tm tmpstm;
			ptm = localtime_r (&(tv.tv_sec), &tmpstm);
#elif HAVE_LOCALTIME_S
			struct tm tmpstm;
			if (localtime_s (&tmpstm, &(tv.tv_sec)) == 0)
				ptm = &tmpstm;
			else
				ptm = NULL;
#else
			ptm = localtime (&(tv.tv_sec));
#endif

			if (!ptm)
				return NULL;

			if (type == GDA_TYPE_TIMESTAMP) {
				GdaTimestamp ts;
				ts.year = ptm->tm_year + 1900;
				ts.month = ptm->tm_mon + 1;
				ts.day = ptm->tm_mday;
				ts.hour = ptm->tm_hour;
				ts.minute = ptm->tm_min;
				ts.second = ptm->tm_sec;
				ts.timezone = GDA_TIMEZONE_INVALID;
				value = gda_value_new (type);
				gda_value_set_timestamp (value, &ts);
			}
			else {
				GDate *date;
				date = g_date_new ();
				g_date_set_time_val (date, &tv);
				value = gda_value_new (type);
				g_value_take_boxed (value, date);
			}
		}
	}
	else if (type == GDA_TYPE_BINARY) {
		GdaBinary *bin;
		bin = g_new (GdaBinary, 1);
		bin->data = g_new (guchar, bv->bv_len);

		bin->binary_length = bv->bv_len;
		memcpy (bin->data, bv->bv_val,
			sizeof (gchar) * bin->binary_length);
		value = gda_value_new (GDA_TYPE_BINARY);
		gda_value_take_binary (value, bin);
	}
	else
		value = gda_value_new_from_string (bv->bv_val, type);

	return value;
}

/*
 * make sure we respect http://www.faqs.org/rfcs/rfc2253.html
 */
static gchar *
rewrite_dn_component (const char *str, guint len)
{
	guint i;
	gint nbrewrite = 0;
	for (i = 0; i < len; i++) {
		// "," / "=" / "+" / "<" /  ">" / "#" / ";"
		if ((str[i] == ',') || (str[i] == '=') || (str[i] == '+') ||
		    (str[i] == '<') || (str[i] == '>') || (str[i] == '#') || (str[i] == ';'))
			nbrewrite++;
	}
	if (nbrewrite == 0)
		return NULL;

	gchar *tmp, *ptr;
	tmp = g_new (gchar, len + 2*nbrewrite + 1);
	for (i = 0, ptr = tmp; i < len; i++) {
		if ((str[i] == ',') || (str[i] == '=') || (str[i] == '+') ||
		    (str[i] == '<') || (str[i] == '>') || (str[i] == '#') || (str[i] == ';')) {
			int t;
			*ptr = '\\';
			ptr++;
			t = str[i] / 16;
			if (t < 10)
				*ptr = '0' + t;
			else
				*ptr = 'A' + t - 10;
			ptr++;
			t = str[i] % 16;
			if (t < 10)
				*ptr = '0' + t;
			else
				*ptr = 'A' + t - 10;
		}
		else
			*ptr = str[i];
		ptr++;
	}
	*ptr = 0;
	return tmp;
}

/**
 * _gda_Rdn2str:
 *
 * Returns: a new string
 */
static gchar *
_gda_Rdn2str (LDAPRDN rdn)
{
	if (!rdn)
		return NULL;
	gint i;
	GString *string = NULL;
	for (i = 0; rdn[i]; i++) {
		LDAPAVA *ava = rdn [i];
		if (g_utf8_validate (ava->la_attr.bv_val, ava->la_attr.bv_len, NULL) &&
		    g_utf8_validate (ava->la_value.bv_val, ava->la_value.bv_len, NULL)) {
			gchar *tmp;
			if (string)
				g_string_append_c (string, '+');
			else
				string = g_string_new ("");

			/* attr name */
			tmp = rewrite_dn_component (ava->la_attr.bv_val, ava->la_attr.bv_len);
			if (tmp) {
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else
				g_string_append_len (string, ava->la_attr.bv_val, ava->la_attr.bv_len);
			g_string_append_c (string, '=');

			/* attr value */
			tmp = rewrite_dn_component (ava->la_value.bv_val, ava->la_value.bv_len);
			if (tmp) {
				g_string_append (string, tmp);
				g_free (tmp);
			}
			else
				g_string_append_len (string, ava->la_value.bv_val, ava->la_value.bv_len);
		}
		else {
			if (string) {
				g_string_free (string, TRUE);
				return NULL;
			}
		}
	}
	return g_string_free (string, FALSE);
}


/**
 * _gda_dn2str:
 *
 * Returns: a new string
 */
static gchar *
_gda_dn2str (LDAPDN dn)
{
	if (!dn)
		return NULL;

	gint i;
	GString *string = NULL;
	for (i = 0; dn[i]; i++) {
		LDAPRDN rdn = dn[i];
		gchar *tmp;
		tmp = _gda_Rdn2str (rdn);
		if (tmp) {
			if (string)
				g_string_append_c (string, ',');
			else
				string = g_string_new ("");
			g_string_append (string, tmp);
			g_free (tmp);
		}
		else {
			if (string) {
				g_string_free (string, TRUE);
				return NULL;
			}
		}
	}
	return g_string_free (string, FALSE);
}

/*
 * parse_dn
 *
 * Parse and reconstruct @attr if @out_userdn is not %NULL
 *
 * Returns: %TRUE if all OK
 */
gboolean
gda_ldap_parse_dn (const char *attr, gchar **out_userdn)
{
	LDAPDN tmpDN;

	if (out_userdn)
		*out_userdn = NULL;

	if (!attr)
		return FALSE;

	/* decoding */
	if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (attr, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return FALSE;
		}
	}

	if (out_userdn) {
		gchar *userdn;
		userdn = _gda_dn2str (tmpDN);
		ldap_dnfree (tmpDN);
		if (userdn)
			*out_userdn = userdn;
		else
			return FALSE;
	}
	else
		ldap_dnfree (tmpDN);

	return TRUE;
}

/*
 * Proxy functions
 */

static gint
attr_array_sort_func (gconstpointer a, gconstpointer b)
{
	GdaLdapAttribute *att1, *att2;
	att1 = *((GdaLdapAttribute**) a);
	att2 = *((GdaLdapAttribute**) b);
	return strcmp (att1->attr_name, att2->attr_name);
}

GdaLdapEntry *
gdaprov_ldap_describe_entry (GdaLdapConnection *cnc, const gchar *dn, GError **error)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (!dn || (dn && *dn), NULL);

        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;

	int res;
	LDAPMessage *msg = NULL;
	const gchar *real_dn;
	real_dn = dn ? dn : cdata->base_dn;
 retry:
	res = ldap_search_ext_s (cdata->handle, real_dn, LDAP_SCOPE_BASE,
				 "(objectClass=*)", NULL, 0,
				 NULL, NULL, NULL, -1,
				 &msg);
	switch (res) {
	case LDAP_SUCCESS:
	case LDAP_NO_SUCH_OBJECT: {
		gint nb_entries;
		LDAPMessage *ldap_row;
		char *attr;
		BerElement* ber;
		GdaLdapEntry *lentry;
		GArray *array = NULL;

		nb_entries = ldap_count_entries (cdata->handle, msg);
		if (nb_entries == 0) {
			ldap_msgfree (msg);
			return NULL;
		}
		else if (nb_entries > 1) {
			g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
				     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
				     _("LDAP server returned more than one entry with DN '%s'"),
				     real_dn);
			return NULL;
		}

		lentry = g_new0 (GdaLdapEntry, 1);
		lentry->dn = g_strdup (real_dn);
		lentry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);
		array = g_array_new (TRUE, FALSE, sizeof (GdaLdapAttribute*));
		ldap_row = ldap_first_entry (cdata->handle, msg);
		for (attr = ldap_first_attribute (cdata->handle, ldap_row, &ber);
		     attr;
		     attr = ldap_next_attribute (cdata->handle, ldap_row, ber)) {
			BerValue **bvals;
			GArray *varray = NULL;
			bvals = ldap_get_values_len (cdata->handle, ldap_row, attr);
			if (bvals) {
				gint i;
				for (i = 0; bvals [i]; i++) {
					if (!varray)
						varray = g_array_new (TRUE, FALSE, sizeof (GValue *));
					GValue *value;
					GType type;
					type = gda_ldap_get_g_type (cdata, attr, NULL);
					/*g_print ("Type for attr %s is %s\n", attr, gda_g_type_to_string (type)); */
					value = gda_ldap_attr_value_to_g_value (cdata, type, bvals[i]);
					g_array_append_val (varray, value);
				}
				ldap_value_free_len (bvals);
			}
			if (varray) {
				GdaLdapAttribute *lattr = NULL;
				lattr = g_new0 (GdaLdapAttribute, 1);
				lattr->attr_name = g_strdup (attr);
				lattr->values = (GValue**) varray->data;
				lattr->nb_values = varray->len;
				g_array_free (varray, FALSE);

				g_array_append_val (array, lattr);
				g_hash_table_insert (lentry->attributes_hash, lattr->attr_name, lattr);
			}
			ldap_memfree (attr);
		}
		if (ber)
			ber_free (ber, 0);
		ldap_msgfree (msg);
		if (array) {
			g_array_sort (array, (GCompareFunc) attr_array_sort_func);
			lentry->attributes = (GdaLdapAttribute**) array->data;
			lentry->nb_attributes = array->len;
			g_array_free (array, FALSE);
		}
		return lentry;
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
		int ldap_errno;
		ldap_get_option (cdata->handle, LDAP_OPT_ERROR_NUMBER, &ldap_errno);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_OTHER_ERROR,
			     "%s", ldap_err2string(ldap_errno));
		return NULL;
	}
	}
}

static gint
entry_array_sort_func (gconstpointer a, gconstpointer b)
{
	GdaLdapEntry *e1, *e2;
	e1 = *((GdaLdapEntry**) a);
	e2 = *((GdaLdapEntry**) b);
	return strcmp (e2->dn, e1->dn);
}

GdaLdapEntry **
gdaprov_ldap_get_entry_children (GdaLdapConnection *cnc, const gchar *dn, gchar **attributes, GError **error)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);
	g_return_val_if_fail (!dn || (dn && *dn), NULL);

        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;

	int res;
	LDAPMessage *msg = NULL;
 retry:
	res = ldap_search_ext_s (cdata->handle, dn ? dn : cdata->base_dn, LDAP_SCOPE_ONELEVEL,
				 "(objectClass=*)", attributes, 0,
				 NULL, NULL, NULL, -1,
				 &msg);

	switch (res) {
	case LDAP_SUCCESS:
	case LDAP_NO_SUCH_OBJECT: {
		LDAPMessage *ldap_row;
		GArray *children;

		children = g_array_new (TRUE, FALSE, sizeof (GdaLdapEntry *));
		for (ldap_row = ldap_first_entry (cdata->handle, msg);
		     ldap_row;
		     ldap_row = ldap_next_entry (cdata->handle, ldap_row)) {
			char *attr;
			GdaLdapEntry *lentry = NULL;
			attr = ldap_get_dn (cdata->handle, ldap_row);
			if (attr) {
				gchar *userdn = NULL;
				if (gda_ldap_parse_dn (attr, &userdn)) {
					lentry = g_new0 (GdaLdapEntry, 1);
					lentry->dn = userdn;
				}
				ldap_memfree (attr);
			}
			
			if (!lentry) {
				gint i;
				for (i = 0; i < children->len; i++) {
					GdaLdapEntry *lentry;
					lentry = g_array_index (children, GdaLdapEntry*, i);
					gda_ldap_entry_free (lentry);
				}
				g_array_free (children, TRUE);
				children = NULL;
				g_set_error (error, GDA_SERVER_PROVIDER_ERROR,
					     GDA_SERVER_PROVIDER_INTERNAL_ERROR,
					     "%s",
					     _("Could not parse distinguished name returned by LDAP server"));
				break;
			}
			else if (attributes) {
				BerElement* ber;
				GArray *array; /* array of GdaLdapAttribute pointers */
				lentry->attributes_hash = g_hash_table_new (g_str_hash, g_str_equal);
				array = g_array_new (TRUE, FALSE, sizeof (GdaLdapAttribute*));
				for (attr = ldap_first_attribute (cdata->handle, ldap_row, &ber);
				     attr;
				     attr = ldap_next_attribute (cdata->handle, ldap_row, ber)) {
					BerValue **bvals;
					GArray *varray = NULL;
					bvals = ldap_get_values_len (cdata->handle, ldap_row, attr);
					if (bvals) {
						gint i;
						for (i = 0; bvals [i]; i++) {
							if (!varray)
								varray = g_array_new (TRUE, FALSE, sizeof (GValue *));
							GValue *value;
							GType type;
							type = gda_ldap_get_g_type (cdata, attr, NULL);
							/*g_print ("%d Type for attr %s is %s\n", i, attr, gda_g_type_to_string (type));*/
							value = gda_ldap_attr_value_to_g_value (cdata, type, bvals[i]);
							g_array_append_val (varray, value);
						}
						ldap_value_free_len (bvals);
					}
					if (varray) {
						GdaLdapAttribute *lattr = NULL;
						lattr = g_new0 (GdaLdapAttribute, 1);
						lattr->attr_name = g_strdup (attr);
						lattr->values = (GValue**) varray->data;
						lattr->nb_values = varray->len;
						g_array_free (varray, FALSE);
						
						g_array_append_val (array, lattr);
						g_hash_table_insert (lentry->attributes_hash, lattr->attr_name, lattr);
					}
					ldap_memfree (attr);
				}
				if (ber)
					ber_free (ber, 0);
				if (array) {
					g_array_sort (array, (GCompareFunc) attr_array_sort_func);
					lentry->attributes = (GdaLdapAttribute**) array->data;
					lentry->nb_attributes = array->len;
					g_array_free (array, FALSE);
				}
			}
			g_array_append_val (children, lentry);
		}
		ldap_msgfree (msg);
		if (children) {
			g_array_sort (children, (GCompareFunc) entry_array_sort_func);
			return (GdaLdapEntry**) g_array_free (children, FALSE);
		}
		else
			return NULL;
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
		int ldap_errno;
		ldap_get_option (cdata->handle, LDAP_OPT_ERROR_NUMBER, &ldap_errno);
		g_set_error (error, GDA_DATA_MODEL_ERROR, GDA_DATA_MODEL_OTHER_ERROR,
			     "%s", ldap_err2string(ldap_errno));
		return NULL;
	}
	}
}

gchar **
gdaprov_ldap_dn_split (const gchar *dn, gboolean all)
{
	LDAPDN tmpDN;
	GArray *array;
	gint imax = G_MAXINT;

	g_return_val_if_fail (dn && *dn, NULL);

	/*g_print ("%s (%s, %d)\n", __FUNCTION__, dn, all);*/

	/* decoding */
	if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return NULL;
		}
	}

	/* encoding */
	array = g_array_new (TRUE, FALSE, sizeof (gchar*));
	if (!all)
		imax = 1;

	gint i;
	LDAPRDN *rdn = tmpDN;
	for (i = 0; rdn [i] && (i < imax); i++) {
		gchar *tmp;
		tmp = _gda_Rdn2str (rdn [i]);
		if (tmp) {
			g_array_append_val (array, tmp);
			/*g_print ("\t[%s]\n", value);*/
		}
		else
			goto onerror;
	}

	if (!all && (i == 1) && rdn [1]) {
		gchar *tmp;
		tmp = _gda_dn2str (rdn+1);
		if (tmp) {
			g_array_append_val (array, tmp);
			/*g_print ("\t[%s]\n", value);*/
		}
		else
			goto onerror;
	}
	ldap_dnfree (tmpDN);

	return (gchar**) g_array_free (array, FALSE);

 onerror:
	for (i = 0; i < array->len; i++) {
		gchar *tmp;
		tmp = g_array_index (array, gchar*, i);
		g_free (tmp);
	}
	g_array_free (array, TRUE);
	return NULL;
}

gboolean
gdaprov_ldap_is_dn (const gchar *dn)
{
	LDAPDN tmpDN;

	g_return_val_if_fail (dn && *dn, FALSE);
	if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV3) != LDAP_SUCCESS) {
		if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_LDAPV2) != LDAP_SUCCESS) {
			if (ldap_str2dn (dn, &tmpDN, LDAP_DN_FORMAT_DCE) != LDAP_SUCCESS)
				return FALSE;
		}
	}
	ldap_dnfree (tmpDN);
	return TRUE;
}

const gchar *
gdaprov_ldap_get_base_dn (GdaLdapConnection *cnc)
{
	LdapConnectionData *cdata;
	g_return_val_if_fail (GDA_IS_LDAP_CONNECTION (cnc), NULL);

        cdata = (LdapConnectionData*) gda_virtual_connection_internal_get_provider_data (GDA_VIRTUAL_CONNECTION (cnc));
        if (!cdata)
                return NULL;
	else
		return cdata->base_dn;
}
