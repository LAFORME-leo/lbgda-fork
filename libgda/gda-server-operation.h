/*
 * Copyright (C) 2006 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2006 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 * Copyright (C) 2010 Jonh Wendell <jwendell@gnome.org>
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

#ifndef __GDA_SERVER_OPERATION_H__
#define __GDA_SERVER_OPERATION_H__

#include <glib-object.h>
#include <libgda/gda-decl.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

#define GDA_TYPE_SERVER_OPERATION            (gda_server_operation_get_type())
#define GDA_SERVER_OPERATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_SERVER_OPERATION, GdaServerOperation))
#define GDA_SERVER_OPERATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_SERVER_OPERATION, GdaServerOperationClass))
#define GDA_IS_SERVER_OPERATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE(obj, GDA_TYPE_SERVER_OPERATION))
#define GDA_IS_SERVER_OPERATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), GDA_TYPE_SERVER_OPERATION))

/*
 * Types of identified operations
 * all the providers don't implement them completely, though
 */
typedef enum {
	GDA_SERVER_OPERATION_CREATE_DB,
	GDA_SERVER_OPERATION_DROP_DB,
		
	GDA_SERVER_OPERATION_CREATE_TABLE,
	GDA_SERVER_OPERATION_DROP_TABLE,
	GDA_SERVER_OPERATION_RENAME_TABLE,

	GDA_SERVER_OPERATION_ADD_COLUMN,
	GDA_SERVER_OPERATION_DROP_COLUMN,

	GDA_SERVER_OPERATION_CREATE_INDEX,
	GDA_SERVER_OPERATION_DROP_INDEX,

	GDA_SERVER_OPERATION_CREATE_VIEW,
	GDA_SERVER_OPERATION_DROP_VIEW,

	GDA_SERVER_OPERATION_COMMENT_TABLE,
	GDA_SERVER_OPERATION_COMMENT_COLUMN,

	GDA_SERVER_OPERATION_CREATE_USER,
	GDA_SERVER_OPERATION_ALTER_USER,
	GDA_SERVER_OPERATION_DROP_USER,

	GDA_SERVER_OPERATION_LAST
} GdaServerOperationType;

/* error reporting */
extern GQuark gda_server_operation_error_quark (void);
#define GDA_SERVER_OPERATION_ERROR gda_server_operation_error_quark ()

typedef enum {
	GDA_SERVER_OPERATION_OBJECT_NAME_ERROR,
	GDA_SERVER_OPERATION_INCORRECT_VALUE_ERROR,
	GDA_SERVER_OPERATION_XML_ERROR
} GdaServerOperationError;

typedef enum
{
	GDA_SERVER_OPERATION_CREATE_TABLE_NOTHING_FLAG   = 1 << 0,
	GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG      = 1 << 1,
	GDA_SERVER_OPERATION_CREATE_TABLE_NOT_NULL_FLAG  = 1 << 2,
	GDA_SERVER_OPERATION_CREATE_TABLE_UNIQUE_FLAG    = 1 << 3,
	GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG   = 1 << 4,
	GDA_SERVER_OPERATION_CREATE_TABLE_FKEY_FLAG      = 1 << 5,
	/* Flags combinations */
	GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_AUTOINC_FLAG = GDA_SERVER_OPERATION_CREATE_TABLE_PKEY_FLAG | GDA_SERVER_OPERATION_CREATE_TABLE_AUTOINC_FLAG
} GdaServerOperationCreateTableFlag;

typedef enum {
	GDA_SERVER_OPERATION_NODE_PARAMLIST,
	GDA_SERVER_OPERATION_NODE_DATA_MODEL,
	GDA_SERVER_OPERATION_NODE_PARAM,
	GDA_SERVER_OPERATION_NODE_SEQUENCE,
	GDA_SERVER_OPERATION_NODE_SEQUENCE_ITEM,

	GDA_SERVER_OPERATION_NODE_DATA_MODEL_COLUMN,
	GDA_SERVER_OPERATION_NODE_UNKNOWN
} GdaServerOperationNodeType;

typedef enum {
	GDA_SERVER_OPERATION_STATUS_OPTIONAL,
	GDA_SERVER_OPERATION_STATUS_REQUIRED,
	GDA_SERVER_OPERATION_STATUS_UNKNOWN
} GdaServerOperationNodeStatus;

typedef struct _GdaServerOperationNode {
	GdaServerOperationNodeType    type;
	GdaServerOperationNodeStatus  status;
	
	GdaSet                       *plist;
	GdaDataModel                 *model;
	GdaColumn                    *column;
	GdaHolder                    *param; 
	gpointer                      priv;
} GdaServerOperationNode;

struct _GdaServerOperation {
	GObject                    object;
	GdaServerOperationPrivate *priv;
};

struct _GdaServerOperationClass {
	GObjectClass               parent_class;

	/* signals */
	void                     (*seq_item_added) (GdaServerOperation *op, const gchar *seq_path, gint item_index);
	void                     (*seq_item_remove) (GdaServerOperation *op, const gchar *seq_path, gint item_index);

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType                      gda_server_operation_get_type                (void) G_GNUC_CONST;
GdaServerOperation        *gda_server_operation_new                     (GdaServerOperationType op_type, const gchar *xml_file);
GdaServerOperationType     gda_server_operation_get_op_type             (GdaServerOperation *op);
const gchar               *gda_server_operation_op_type_to_string       (GdaServerOperationType type);
GdaServerOperationType     gda_server_operation_string_to_op_type       (const gchar *str);
GdaServerOperationNode    *gda_server_operation_get_node_info           (GdaServerOperation *op, const gchar *path_format, ...);

const GValue              *gda_server_operation_get_value_at            (GdaServerOperation *op, const gchar *path_format, ...);
const GValue              *gda_server_operation_get_value_at_path       (GdaServerOperation *op, const gchar *path);
gchar                     *gda_server_operation_get_sql_identifier_at   (GdaServerOperation *op,
									 GdaConnection *cnc, GdaServerProvider *prov,
									 const gchar *path_format, ...);    
gchar                     *gda_server_operation_get_sql_identifier_at_path (GdaServerOperation *op, 
									    GdaConnection *cnc, GdaServerProvider *prov,
									    const gchar *path);
gboolean                   gda_server_operation_set_value_at            (GdaServerOperation *op, const gchar *value, 
									 GError **error, const gchar *path_format, ...);
gboolean                   gda_server_operation_set_value_at_path       (GdaServerOperation *op, const gchar *value, 
									 const gchar *path, GError **error);

xmlNodePtr                 gda_server_operation_save_data_to_xml        (GdaServerOperation *op, GError **error);
gboolean                   gda_server_operation_load_data_from_xml      (GdaServerOperation *op, 
									 xmlNodePtr node, GError **error);

gchar**                    gda_server_operation_get_root_nodes          (GdaServerOperation *op);
GdaServerOperationNodeType gda_server_operation_get_node_type           (GdaServerOperation *op, const gchar *path,
								         GdaServerOperationNodeStatus *status);
gchar                     *gda_server_operation_get_node_parent         (GdaServerOperation *op, const gchar *path);
gchar                     *gda_server_operation_get_node_path_portion   (GdaServerOperation *op, const gchar *path);

const gchar               *gda_server_operation_get_sequence_name       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_size       (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_max_size   (GdaServerOperation *op, const gchar *path);
guint                      gda_server_operation_get_sequence_min_size   (GdaServerOperation *op, const gchar *path);
gchar                    **gda_server_operation_get_sequence_item_names (GdaServerOperation *op, const gchar *path); 

guint                      gda_server_operation_add_item_to_sequence    (GdaServerOperation *op, const gchar *seq_path);
gboolean                   gda_server_operation_del_item_from_sequence  (GdaServerOperation *op, const gchar *item_path);

gboolean                   gda_server_operation_is_valid                (GdaServerOperation *op, const gchar *xml_file, GError **error);

/*
 * Database creation and destruction
 */
GdaServerOperation *gda_server_operation_prepare_create_database       (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_server_operation_perform_create_database       (GdaServerOperation *op, const gchar *provider, GError **error);
GdaServerOperation *gda_server_operation_prepare_drop_database         (const gchar *provider, const gchar *db_name, GError **error);
gboolean            gda_server_operation_perform_drop_database         (GdaServerOperation *op, const gchar *provider, GError **error);

/*
 * Tables creation and destruction
 */
GdaServerOperation *gda_server_operation_prepare_create_table	      (GdaConnection *cnc, const gchar *table_name, GError **error, ...);
gboolean            gda_server_operation_perform_create_table          (GdaServerOperation *op, GError **error);
GdaServerOperation *gda_server_operation_prepare_drop_table            (GdaConnection *cnc, const gchar *table_name, GError **error);
gboolean            gda_server_operation_perform_drop_table            (GdaServerOperation *op, GError **error);

G_END_DECLS

#endif
