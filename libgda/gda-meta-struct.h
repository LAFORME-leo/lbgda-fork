/*
 * Copyright (C) 2008 - 2011 Vivien Malerba <malerba@gnome-db.org>
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

#ifndef __GDA_META_STRUCT_H_
#define __GDA_META_STRUCT_H_

#include <glib-object.h>
#include <libgda/gda-data-model.h>
#include <libgda/gda-meta-store.h>
#include <libgda/gda-decl.h>
#include <libgda/gda-attributes-manager.h>

G_BEGIN_DECLS

#define GDA_TYPE_META_STRUCT          (gda_meta_struct_get_type())
#define GDA_META_STRUCT(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, gda_meta_struct_get_type(), GdaMetaStruct)
#define GDA_META_STRUCT_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, gda_meta_struct_get_type (), GdaMetaStructClass)
#define GDA_IS_META_STRUCT(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, gda_meta_struct_get_type ())

/* error reporting */
extern GQuark gda_meta_struct_error_quark (void);
#define GDA_META_STRUCT_ERROR gda_meta_struct_error_quark ()

typedef enum {
	GDA_META_STRUCT_UNKNOWN_OBJECT_ERROR,
        GDA_META_STRUCT_DUPLICATE_OBJECT_ERROR,
        GDA_META_STRUCT_INCOHERENCE_ERROR,
	GDA_META_STRUCT_XML_ERROR
} GdaMetaStructError;


/* struct for the object's data */
struct _GdaMetaStruct
{
	GObject                object;
	GdaMetaStructPrivate  *priv;
};

/* struct for the object's class */
struct _GdaMetaStructClass
{
	GObjectClass              parent_class;

	/*< private >*/
	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

/**
 * GdaMetaDbObjectType:
 * @GDA_META_DB_UNKNOWN: unknown type
 * @GDA_META_DB_TABLE: represents a table
 * @GDA_META_DB_VIEW: represents a view
 *
 * Type of database object which can be handled as a #GdaMetaDbObject
 */
typedef enum {
	GDA_META_DB_UNKNOWN,
	GDA_META_DB_TABLE,
	GDA_META_DB_VIEW
} GdaMetaDbObjectType;

/*
 * Controls which features are computed about database objects
 */
typedef enum {
	GDA_META_STRUCT_FEATURE_NONE              = 0,
	GDA_META_STRUCT_FEATURE_FOREIGN_KEYS      = 1 << 0,
	GDA_META_STRUCT_FEATURE_VIEW_DEPENDENCIES = 1 << 1,

	GDA_META_STRUCT_FEATURE_ALL               = GDA_META_STRUCT_FEATURE_FOREIGN_KEYS |
	                                            GDA_META_STRUCT_FEATURE_VIEW_DEPENDENCIES
} GdaMetaStructFeature;

/**
 * GdaMetaSortType:
 * @GDA_META_SORT_ALHAPETICAL: sort alphabetically
 * @GDA_META_SORT_DEPENDENCIES: sort by dependencies
 *
 * Types of sorting
 */
typedef enum {
	GDA_META_SORT_ALHAPETICAL,
	GDA_META_SORT_DEPENDENCIES
} GdaMetaSortType;

/**
 * GdaMetaTable:
 * @columns: (element-type Gda.MetaTableColumn): list of #GdaMetaTableColumn structures, one for each column in the table
 * @pk_cols_array: index of the columns part of the primary key for the table (WARNING: columns numbering
 *                 here start at 0)
 * @pk_cols_nb: size of the @pk_cols_array array
 * @reverse_fk_list: (element-type Gda.MetaTableForeignKey): list of #GdaMetaTableForeignKey where the referenced table is this table
 * @fk_list: (element-type Gda.MetaTableForeignKey): list of #GdaMetaTableForeignKey for this table
 *
 * This structure specifies a #GdaMetaDbObject to represent a table's specific attributes,
 * its contents must not be modified.
 *
 * Note that in some cases, the columns cannot be determined for views, and in this case the
 * @columns will be %NULL (this can be the case for example with SQLite where a view
 * uses a function which is not natively provided by SQLite.
 */
typedef struct {
	/*< public >*/
	GSList       *columns;
	
	/* PK fields index */
	gint         *pk_cols_array;
	gint          pk_cols_nb;

	/* Foreign keys */
	GSList       *reverse_fk_list; /* list of GdaMetaTableForeignKey where @depend_on == this GdaMetaDbObject */
	GSList       *fk_list; /* list of GdaMetaTableForeignKey where @meta_table == this GdaMetaDbObject */

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaMetaTable;

/**
 * GdaMetaView:
 * @table: a view is also a table as it has columns
 * @view_def: views' definition
 * @is_updatable: tells if the view's contents can be updated
 *
 * This structure specifies a #GdaMetaDbObject to represent a view's specific attributes,
 * its contents must not be modified.
 */
typedef struct {
	/*< public >*/
	GdaMetaTable  table;
	gchar        *view_def;
	gboolean      is_updatable;

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaMetaView;

/**
 * GdaMetaDbObject:
 * @extra: union for the actual object's contents, to be able to cast it using GDA_META_TABLE(), GDA_META_VIEW()
 * @obj_type: the type of object (table, view)
 * @outdated: set to %TRUE if the information in this #GdaMetaDbObject may be outdated because the #GdaMetaStore has been updated
 * @obj_catalog: the catalog the object is in
 * @obj_schema: the schema the object is in
 * @obj_name: the object's name
 * @obj_short_name: the shortest way to name the object
 * @obj_full_name: the full name of the object (in the &lt;schema&gt;.&lt;nameagt; notation
 * @obj_owner: object's owner
 * @depend_list: (element-type Gda.MetaDbObject): list of #GdaMetaDbObject pointers on which this object depends (through foreign keys
 *               or tables used for views)
 *
 * Struture to hold information about each database object (tables, views, ...),
 * its contents must not be modified.
 *
 * Note: @obj_catalog, @obj_schema, @obj_name, @obj_short_name and @obj_full_name respect the
 * <link linkend="information_schema:sql_identifiers">SQL identifiers</link> convention used in
 * #GdaMetaStore objects. Before using these SQL identifiers, you should check the
 * gda_sql_identifier_quote() to know if is it is necessary to surround by double quotes
 * before using in an SQL statement.
 */
typedef struct {
	/*< public >*/
	union {
		GdaMetaTable    meta_table;
		GdaMetaView     meta_view;
	}                       extra;
	GdaMetaDbObjectType     obj_type;
	gboolean                outdated;
	gchar                  *obj_catalog;
	gchar                  *obj_schema;
	gchar                  *obj_name;
	gchar                  *obj_short_name;
	gchar                  *obj_full_name;
	gchar                  *obj_owner;

	GSList                 *depend_list;

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaMetaDbObject;

/**
 * GDA_META_DB_OBJECT:
 * @dbo: a pointer
 *
 * Casts @dbo to a #GdaMetaDbObject, no check is made on the validity of @dbo
 *
 * Returns: a pointer to a #GdaMetaDbObject
 */
#define GDA_META_DB_OBJECT(dbo) ((GdaMetaDbObject*)(dbo))

/**
 * GDA_META_TABLE:
 * @dbo: a pointer
 *
 * Casts @dbo to a #GdaMetaTable, no check is made on the validity of @dbo
 *
 * Returns: a pointer to a #GdaMetaTable
 */
#define GDA_META_TABLE(dbobj) (&((dbobj)->extra.meta_table))

/**
 * GDA_META_VIEW:
 * @dbo: a pointer
 *
 * Casts @dbo to a #GdaMetaView, no check is made on the validity of @dbo
 *
 * Returns: a pointer to a #GdaMetaView
 */
#define GDA_META_VIEW(dbobj) (&((dbobj)->extra.meta_view))

/**
 * GdaMetaTableColumn:
 * @column_name: the column's name
 * @column_type: the column's DBMS's type
 * @gtype: the detected column's #GType
 * @pkey: tells if the column is part of a primary key
 * @nullok: tells if the column can be %NULL
 * @default_value: (allow-none): the column's default value, represented as a valid SQL value (surrounded by simple quotes for strings, ...), or %NULL if column has no default value
 *
 * This structure represents a table of view's column, its contents must not be modified.
 */
typedef struct {
	/*< public >*/
	gchar        *column_name;
	gchar        *column_type;
	GType         gtype;
	gboolean      pkey;
	gboolean      nullok;
	gchar        *default_value;

	/*< private >*/
	/* Padding for future expansion */
	gpointer _gda_reserved1;
	gpointer _gda_reserved2;
	gpointer _gda_reserved3;
	gpointer _gda_reserved4;
} GdaMetaTableColumn;

/**
 * GDA_META_TABLE_COLUMN:
 * @col: a pointer
 *
 * Casts @col to a #GdaMetaTableColumn, no check is made
 *
 * Returns: @col, casted to a #GdaMetaTableColumn
 */
#define GDA_META_TABLE_COLUMN(col) ((GdaMetaTableColumn*)(col))

const GValue *gda_meta_table_column_get_attribute (GdaMetaTableColumn *tcol, const gchar *attribute);
void          gda_meta_table_column_set_attribute (GdaMetaTableColumn *tcol, const gchar *attribute, const GValue *value,
						   GDestroyNotify destroy);
/**
 * gda_meta_table_column_set_attribute_static:
 * @column: a #GdaMetaTableColumn
 * @attribute: attribute's name
 * @value: (allow-none): a #GValue, or %NULL
 *
 * This function is similar to gda_meta_table_column_set_attribute() but for static strings
 */
#define gda_meta_table_column_set_attribute_static(column,attribute,value) gda_meta_table_column_set_attribute((column),(attribute),(value),NULL)

void          gda_meta_table_column_foreach_attribute (GdaMetaTableColumn *tcol, GdaAttributesManagerFunc func, gpointer data);

/**
 * GdaMetaForeignKeyPolicy:
 * @GDA_META_FOREIGN_KEY_UNKNOWN: unspecified policy
 * @GDA_META_FOREIGN_KEY_NONE: not enforced policy
 * @GDA_META_FOREIGN_KEY_NO_ACTION: return an error, no action taken
 * @GDA_META_FOREIGN_KEY_RESTRICT: same as @GDA_META_FOREIGN_KEY_NO_ACTION, not deferrable
 * @GDA_META_FOREIGN_KEY_CASCADE: policy is to delete any rows referencing the deleted row, or update the value of the referencing column to the new value of the referenced column, respectively
 * @GDA_META_FOREIGN_KEY_SET_NULL: policy is to set the referencing column to NULL
 * @GDA_META_FOREIGN_KEY_SET_DEFAULT: policy is to set the referencing column to its default value
 *
 * Defines the filtering policy of a foreign key when invoked on an UPDATE
 * or DELETE operation.
 */
typedef enum {
	GDA_META_FOREIGN_KEY_UNKNOWN,
	GDA_META_FOREIGN_KEY_NONE,
	GDA_META_FOREIGN_KEY_NO_ACTION,
	GDA_META_FOREIGN_KEY_RESTRICT,
	GDA_META_FOREIGN_KEY_CASCADE,
	GDA_META_FOREIGN_KEY_SET_NULL,
	GDA_META_FOREIGN_KEY_SET_DEFAULT
} GdaMetaForeignKeyPolicy;

/**
 * GdaMetaTableForeignKey:
 * @meta_table: the #GdaMetaDbObject for which this structure represents a foreign key
 * @depend_on: the #GdaMetaDbObject which is referenced by the foreign key
 * @cols_nb: the size of the @fk_cols_array, @fk_names_array, @ref_pk_cols_array and @ref_pk_names_array arrays
 * @fk_cols_array: the columns' indexes in @meta_table which participate in the constraint (WARNING: columns numbering
 *                 here start at 1)
 * @fk_names_array: the columns' names in @meta_table which participate in the constraint
 * @ref_pk_cols_array: the columns' indexes in @depend_on which participate in the constraint (WARNING: columns numbering
 *                 here start at 1)
 * @ref_pk_names_array: the columns' names in @depend_on which participate in the constraint
 *
 * This structure represents a foreign key constraint, its contents must not be modified.
 */
typedef struct {
	/*< public >*/
	GdaMetaDbObject  *meta_table;
	GdaMetaDbObject  *depend_on;

	gint              cols_nb;	
	gint             *fk_cols_array; /* FK fields index */
	gchar           **fk_names_array; /* FK fields names */
	gint             *ref_pk_cols_array; /* Ref PK fields index */
	gchar           **ref_pk_names_array; /* Ref PK fields names */

	/*< private >*/
	gpointer          on_update_policy; /* pointer containing the GdaMetaForeignKeyPolicy integer
					     * to keep ABI from 4.0, use GINT_TO_POINTER and
					     * GPOINTER_TO_INT */
	gpointer          on_delete_policy; /* pointer containing the GdaMetaForeignKeyPolicy integer
					     * to keep ABI from 4.0, use GINT_TO_POINTER and
					     * GPOINTER_TO_INT */
	gpointer          declared; /* pointer to a boolean to keep ABI from 4.0.
				     * Any non NULL if FK has been declared in meta data */

	/*< public >*/
	gchar            *fk_name;
} GdaMetaTableForeignKey;
/**
 * GDA_META_TABLE_FOREIGN_KEY:
 * @fk: a pointer
 *
 * Casts @fk to a #GdaMetaTableForeignKey (no check is actuelly being done on @fk's validity)
 *
 * Returns: @col, casted to a #GdaMetaTableForeignKey
 */
#define GDA_META_TABLE_FOREIGN_KEY(fk) ((GdaMetaTableForeignKey*)(fk))

/**
 * GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY:
 * @fk: a pointer to a #GdaMetaTableForeignKey
 * 
 * Tells the actual policy implemented by @fk when used in the context of an UPDATE.
 *
 * Returns: the policy as a #GdaMetaForeignKeyPolicy
 */
#define GDA_META_TABLE_FOREIGN_KEY_ON_UPDATE_POLICY(fk) ((GdaMetaForeignKeyPolicy) GPOINTER_TO_INT ((GdaMetaTableForeignKey*)(fk)->on_update_policy))

/**
 * GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY:
 * @fk: a pointer to a #GdaMetaTableForeignKey
 * 
 * Tells the actual policy implemented by @fk when used in the context of a DELETE.
 *
 * Returns: the policy as a #GdaMetaForeignKeyPolicy
 */
#define GDA_META_TABLE_FOREIGN_KEY_ON_DELETE_POLICY(fk) ((GdaMetaForeignKeyPolicy) GPOINTER_TO_INT ((GdaMetaTableForeignKey*)(fk)->on_delete_policy))

/**
 * GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED:
 * @fk: a pointer to a #GdaMetaTableForeignKey
 *
 * Tells if @fk is an actual foreign key defined in the database's schema, or if it is an indication which
 * has been added to help Libgda understand the database schema.
 *
 * Returns: %TRUE if @fk has been declared in the database's meta data and %FALSE if @fk is an actual foreign key defined in the database's schema
 */
#define GDA_META_TABLE_FOREIGN_KEY_IS_DECLARED(fk) ((((GdaMetaTableForeignKey*)(fk))->declared) ? TRUE : FALSE)

GType               gda_meta_struct_get_type           (void) G_GNUC_CONST;
GdaMetaStruct      *gda_meta_struct_new                (GdaMetaStore *store, GdaMetaStructFeature features);
GdaMetaDbObject    *gda_meta_struct_complement         (GdaMetaStruct *mstruct, GdaMetaDbObjectType type,
							const GValue *catalog, const GValue *schema, const GValue *name,
							GError **error);
gboolean            gda_meta_struct_complement_schema  (GdaMetaStruct *mstruct,
							const GValue *catalog, const GValue *schema, GError **error);
gboolean            gda_meta_struct_complement_default (GdaMetaStruct *mstruct, GError **error);
gboolean            gda_meta_struct_complement_all     (GdaMetaStruct *mstruct, GError **error);
gboolean            gda_meta_struct_complement_depend  (GdaMetaStruct *mstruct, GdaMetaDbObject *dbo,
							GError **error);

gboolean            gda_meta_struct_sort_db_objects    (GdaMetaStruct *mstruct, GdaMetaSortType sort_type, GError **error);
GSList             *gda_meta_struct_get_all_db_objects (GdaMetaStruct *mstruct);
GdaMetaDbObject    *gda_meta_struct_get_db_object      (GdaMetaStruct *mstruct,
						        const GValue *catalog, const GValue *schema, const GValue *name);
GdaMetaTableColumn *gda_meta_struct_get_table_column   (GdaMetaStruct *mstruct, GdaMetaTable *table,
						        const GValue *col_name);

typedef enum {
	GDA_META_GRAPH_COLUMNS = 1 << 0
} GdaMetaGraphInfo;

gchar              *gda_meta_struct_dump_as_graph      (GdaMetaStruct *mstruct, GdaMetaGraphInfo info, GError **error);

G_END_DECLS

#endif
