/*
 * Copyright (C) 2009 Vivien Malerba <malerba@gnome-db.org>
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




/*
 * TABLE: _attributes
 *
 * Table to store (key,value) pairs (keys starting with '_' are reserved)
 */
static GType _col_types_attributes[] = {
  G_TYPE_STRING  /* column: att_name */
, G_TYPE_STRING  /* column: att_value */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _information_schema_catalog_name
 *
 * Name of the current database (current catalog), has only one row
 */
static GType _col_types_information_schema_catalog_name[] = {
  G_TYPE_STRING  /* column: catalog_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _schemata
 *
 * List of schemas
 */
static GType _col_types_schemata[] = {
  G_TYPE_STRING  /* column: catalog_name */
, G_TYPE_STRING  /* column: schema_name */
, G_TYPE_STRING  /* column: schema_owner */
, G_TYPE_BOOLEAN  /* column: schema_internal */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _builtin_data_types
 *
 * List of built-in data types such as varchar, int, ...
 */
static GType _col_types_builtin_data_types[] = {
  G_TYPE_STRING  /* column: short_type_name */
, G_TYPE_STRING  /* column: full_type_name */
, G_TYPE_STRING  /* column: gtype */
, G_TYPE_STRING  /* column: comments */
, G_TYPE_STRING  /* column: synonyms */
, G_TYPE_BOOLEAN  /* column: internal */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _udt
 *
 * User defined data types
 */
static GType _col_types_udt[] = {
  G_TYPE_STRING  /* column: udt_catalog */
, G_TYPE_STRING  /* column: udt_schema */
, G_TYPE_STRING  /* column: udt_name */
, G_TYPE_STRING  /* column: udt_gtype */
, G_TYPE_STRING  /* column: udt_comments */
, G_TYPE_STRING  /* column: udt_short_name */
, G_TYPE_STRING  /* column: udt_full_name */
, G_TYPE_BOOLEAN  /* column: udt_internal */
, G_TYPE_STRING  /* column: udt_owner */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _udt_columns
 *
 * List of components for a user defined data type for composed data types (such as a complex number data type which has real and imaginary parts)
 */
static GType _col_types_udt_columns[] = {
  G_TYPE_STRING  /* column: udt_catalog */
, G_TYPE_STRING  /* column: udt_schema */
, G_TYPE_STRING  /* column: udt_name */
, G_TYPE_STRING  /* column: udt_column */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_INT  /* column: character_maximum_length */
, G_TYPE_INT  /* column: character_octet_length */
, G_TYPE_INT  /* column: numeric_precision */
, G_TYPE_INT  /* column: numeric_scale */
, G_TYPE_INT  /* column: datetime_precision */
, G_TYPE_STRING  /* column: character_set_catalog */
, G_TYPE_STRING  /* column: character_set_schema */
, G_TYPE_STRING  /* column: character_set_name */
, G_TYPE_STRING  /* column: collation_catalog */
, G_TYPE_STRING  /* column: collation_schema */
, G_TYPE_STRING  /* column: collation_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _enums
 *
 * List of possible enumeration labels for enumerations
 */
static GType _col_types_enums[] = {
  G_TYPE_STRING  /* column: udt_catalog */
, G_TYPE_STRING  /* column: udt_schema */
, G_TYPE_STRING  /* column: udt_name */
, G_TYPE_STRING  /* column: label */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _element_types
 *
 * Array specific attributes for array data types
 */
static GType _col_types_element_types[] = {
  G_TYPE_STRING  /* column: specific_name */
, G_TYPE_STRING  /* column: object_catalog */
, G_TYPE_STRING  /* column: object_schema */
, G_TYPE_STRING  /* column: object_name */
, G_TYPE_STRING  /* column: object_type */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_INT  /* column: min_cardinality */
, G_TYPE_INT  /* column: max_cardinality */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _domains
 *
 * List of domains
 */
static GType _col_types_domains[] = {
  G_TYPE_STRING  /* column: domain_catalog */
, G_TYPE_STRING  /* column: domain_schema */
, G_TYPE_STRING  /* column: domain_name */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_STRING  /* column: domain_gtype */
, G_TYPE_INT  /* column: character_maximum_length */
, G_TYPE_INT  /* column: character_octet_length */
, G_TYPE_STRING  /* column: collation_catalog */
, G_TYPE_STRING  /* column: collation_schema */
, G_TYPE_STRING  /* column: collation_name */
, G_TYPE_STRING  /* column: character_set_catalog */
, G_TYPE_STRING  /* column: character_set_schema */
, G_TYPE_STRING  /* column: character_set_name */
, G_TYPE_INT  /* column: numeric_precision */
, G_TYPE_INT  /* column: numeric_scale */
, G_TYPE_STRING  /* column: domain_default */
, G_TYPE_STRING  /* column: domain_comments */
, G_TYPE_STRING  /* column: domain_short_name */
, G_TYPE_STRING  /* column: domain_full_name */
, G_TYPE_BOOLEAN  /* column: domain_internal */
, G_TYPE_STRING  /* column: domain_owner */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _tables
 *
 * List of tables (tables, views or other objects which can contain data)
 */
static GType _col_types_tables[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: table_type */
, G_TYPE_BOOLEAN  /* column: is_insertable_into */
, G_TYPE_STRING  /* column: table_comments */
, G_TYPE_STRING  /* column: table_short_name */
, G_TYPE_STRING  /* column: table_full_name */
, G_TYPE_STRING  /* column: table_owner */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _views
 *
 * List of views and their specific information
 */
static GType _col_types_views[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: view_definition */
, G_TYPE_STRING  /* column: check_option */
, G_TYPE_BOOLEAN  /* column: is_updatable */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _collations
 *
 * List of collations methods
 */
static GType _col_types_collations[] = {
  G_TYPE_STRING  /* column: collation_catalog */
, G_TYPE_STRING  /* column: collation_schema */
, G_TYPE_STRING  /* column: collation_name */
, G_TYPE_STRING  /* column: collation_comments */
, G_TYPE_STRING  /* column: collation_short_name */
, G_TYPE_STRING  /* column: collation_full_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _character_sets
 *
 * List of character sets
 */
static GType _col_types_character_sets[] = {
  G_TYPE_STRING  /* column: character_set_catalog */
, G_TYPE_STRING  /* column: character_set_schema */
, G_TYPE_STRING  /* column: character_set_name */
, G_TYPE_STRING  /* column: default_collate_catalog */
, G_TYPE_STRING  /* column: default_collate_schema */
, G_TYPE_STRING  /* column: default_collate_name */
, G_TYPE_STRING  /* column: character_set_comments */
, G_TYPE_STRING  /* column: character_set_short_name */
, G_TYPE_STRING  /* column: character_set_full_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _routines
 *
 * List of functions and stored procedures (note: the primary jey for that table is composed of (specific_catalog, specific_schema, specific_name))
 */
static GType _col_types_routines[] = {
  G_TYPE_STRING  /* column: specific_catalog */
, G_TYPE_STRING  /* column: specific_schema */
, G_TYPE_STRING  /* column: specific_name */
, G_TYPE_STRING  /* column: routine_catalog */
, G_TYPE_STRING  /* column: routine_schema */
, G_TYPE_STRING  /* column: routine_name */
, G_TYPE_STRING  /* column: routine_type */
, G_TYPE_STRING  /* column: return_type */
, G_TYPE_BOOLEAN  /* column: returns_set */
, G_TYPE_INT  /* column: nb_args */
, G_TYPE_STRING  /* column: routine_body */
, G_TYPE_STRING  /* column: routine_definition */
, G_TYPE_STRING  /* column: external_name */
, G_TYPE_STRING  /* column: external_language */
, G_TYPE_STRING  /* column: parameter_style */
, G_TYPE_BOOLEAN  /* column: is_deterministic */
, G_TYPE_STRING  /* column: sql_data_access */
, G_TYPE_BOOLEAN  /* column: is_null_call */
, G_TYPE_STRING  /* column: routine_comments */
, G_TYPE_STRING  /* column: routine_short_name */
, G_TYPE_STRING  /* column: routine_full_name */
, G_TYPE_STRING  /* column: routine_owner */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _triggers
 *
 * List of triggers
 */
static GType _col_types_triggers[] = {
  G_TYPE_STRING  /* column: trigger_catalog */
, G_TYPE_STRING  /* column: trigger_schema */
, G_TYPE_STRING  /* column: trigger_name */
, G_TYPE_STRING  /* column: event_manipulation */
, G_TYPE_STRING  /* column: event_object_catalog */
, G_TYPE_STRING  /* column: event_object_schema */
, G_TYPE_STRING  /* column: event_object_table */
, G_TYPE_STRING  /* column: action_statement */
, G_TYPE_STRING  /* column: action_orientation */
, G_TYPE_STRING  /* column: condition_timing */
, G_TYPE_STRING  /* column: trigger_comments */
, G_TYPE_STRING  /* column: trigger_short_name */
, G_TYPE_STRING  /* column: trigger_full_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _columns
 *
 * List of columns composing tables
 */
static GType _col_types_columns[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_STRING  /* column: column_default */
, G_TYPE_BOOLEAN  /* column: is_nullable */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_STRING  /* column: gtype */
, G_TYPE_INT  /* column: character_maximum_length */
, G_TYPE_INT  /* column: character_octet_length */
, G_TYPE_INT  /* column: numeric_precision */
, G_TYPE_INT  /* column: numeric_scale */
, G_TYPE_INT  /* column: datetime_precision */
, G_TYPE_STRING  /* column: character_set_catalog */
, G_TYPE_STRING  /* column: character_set_schema */
, G_TYPE_STRING  /* column: character_set_name */
, G_TYPE_STRING  /* column: collation_catalog */
, G_TYPE_STRING  /* column: collation_schema */
, G_TYPE_STRING  /* column: collation_name */
, G_TYPE_STRING  /* column: extra */
, G_TYPE_BOOLEAN  /* column: is_updatable */
, G_TYPE_STRING  /* column: column_comments */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _table_constraints
 *
 * List of constraints applied to tables (Check, primary or foreign key, or unique constraints)
 */
static GType _col_types_table_constraints[] = {
  G_TYPE_STRING  /* column: constraint_catalog */
, G_TYPE_STRING  /* column: constraint_schema */
, G_TYPE_STRING  /* column: constraint_name */
, G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: constraint_type */
, G_TYPE_STRING  /* column: check_clause */
, G_TYPE_BOOLEAN  /* column: is_deferrable */
, G_TYPE_BOOLEAN  /* column: initially_deferred */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _referential_constraints
 *
 * List of foreign key constraints, along with some specific attributes
 */
static GType _col_types_referential_constraints[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: constraint_name */
, G_TYPE_STRING  /* column: ref_table_catalog */
, G_TYPE_STRING  /* column: ref_table_schema */
, G_TYPE_STRING  /* column: ref_table_name */
, G_TYPE_STRING  /* column: ref_constraint_name */
, G_TYPE_STRING  /* column: match_option */
, G_TYPE_STRING  /* column: update_rule */
, G_TYPE_STRING  /* column: delete_rule */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _key_column_usage
 *
 * List of primary key constraints and the name of the tables' columns involved
 */
static GType _col_types_key_column_usage[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: constraint_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _check_column_usage
 *
 * List of check constraints and the name of the tables' columns involved
 */
static GType _col_types_check_column_usage[] = {
  G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: constraint_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _view_column_usage
 *
 * List of the tables' columns involved in a view
 */
static GType _col_types_view_column_usage[] = {
  G_TYPE_STRING  /* column: view_catalog */
, G_TYPE_STRING  /* column: view_schema */
, G_TYPE_STRING  /* column: view_name */
, G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _domain_constraints
 *
 * List of constraints applicable to domains
 */
static GType _col_types_domain_constraints[] = {
  G_TYPE_STRING  /* column: constraint_catalog */
, G_TYPE_STRING  /* column: constraint_schema */
, G_TYPE_STRING  /* column: constraint_name */
, G_TYPE_STRING  /* column: domain_catalog */
, G_TYPE_STRING  /* column: domain_schema */
, G_TYPE_STRING  /* column: domain_name */
, G_TYPE_STRING  /* column: check_clause */
, G_TYPE_BOOLEAN  /* column: is_deferrable */
, G_TYPE_BOOLEAN  /* column: initially_deferred */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _parameters
 *
 * List of routines' (functions and stored procedures) parameters (may not contain data for some routines which accept any type of parameter)
 */
static GType _col_types_parameters[] = {
  G_TYPE_STRING  /* column: specific_catalog */
, G_TYPE_STRING  /* column: specific_schema */
, G_TYPE_STRING  /* column: specific_name */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_STRING  /* column: parameter_mode */
, G_TYPE_STRING  /* column: parameter_name */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _routine_columns
 *
 * List of routines' (functions and stored procedures) returned values' parts (columns) for routines returning composed values
 */
static GType _col_types_routine_columns[] = {
  G_TYPE_STRING  /* column: specific_catalog */
, G_TYPE_STRING  /* column: specific_schema */
, G_TYPE_STRING  /* column: specific_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_STRING  /* column: data_type */
, G_TYPE_STRING  /* column: array_spec */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _table_indexes
 *
 * List of tables' indexes which do not relate to primary keys
 */
static GType _col_types_table_indexes[] = {
  G_TYPE_STRING  /* column: index_catalog */
, G_TYPE_STRING  /* column: index_schema */
, G_TYPE_STRING  /* column: index_name */
, G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_BOOLEAN  /* column: is_unique */
, G_TYPE_STRING  /* column: index_def */
, G_TYPE_STRING  /* column: index_type */
, G_TYPE_STRING  /* column: extra */
, G_TYPE_STRING  /* column: index_owner */
, G_TYPE_STRING  /* column: index_comments */
, G_TYPE_NONE /* end of array marker */
};



/*
 * TABLE: _index_column_usage
 *
 * List of the tables' columns involved in an index listed in the _table_indexes table
 */
static GType _col_types_index_column_usage[] = {
  G_TYPE_STRING  /* column: index_catalog */
, G_TYPE_STRING  /* column: index_schema */
, G_TYPE_STRING  /* column: index_name */
, G_TYPE_STRING  /* column: table_catalog */
, G_TYPE_STRING  /* column: table_schema */
, G_TYPE_STRING  /* column: table_name */
, G_TYPE_STRING  /* column: column_name */
, G_TYPE_STRING  /* column: column_expr */
, G_TYPE_INT  /* column: ordinal_position */
, G_TYPE_NONE /* end of array marker */
};

