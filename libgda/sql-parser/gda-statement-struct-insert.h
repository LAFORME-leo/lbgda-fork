/*
 * Copyright (C) 2008 - 2009 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2009 Murray Cumming <murrayc@murrayc.com>
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

#ifndef _GDA_STATEMENT_STRUCT_INSERT_H_
#define _GDA_STATEMENT_STRUCT_INSERT_H_

#include <glib.h>
#include <glib-object.h>
#include <sql-parser/gda-statement-struct-decl.h>
#include <sql-parser/gda-statement-struct-select.h>
#include <sql-parser/gda-statement-struct-parts.h>

G_BEGIN_DECLS

/*
 * Structure definition
 */
struct _GdaSqlStatementInsert {
	GdaSqlAnyPart           any;
	gchar                  *on_conflict; /* conflict resolution clause */
	GdaSqlTable            *table;
	GSList                 *fields_list; /* list of GdaSqlField structures */
	GSList                 *values_list; /* list of list of GdaSqlExpr */
	GdaSqlAnyPart          *select; /* SELECT OR COMPOUND statements: GdaSqlStatementSelect or GdaSqlStatementCompound */

	/* Padding for future expansion */
	gpointer         _gda_reserved1;
	gpointer         _gda_reserved2;
};

/*
 * Common operations
 */
GdaSqlStatementContentsInfo *_gda_sql_statement_insert_get_infos (void);

/*
 * Functions used by the parser
 */
void gda_sql_statement_insert_take_table_name (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_insert_take_on_conflict (GdaSqlStatement *stmt, GValue *value);
void gda_sql_statement_insert_take_fields_list (GdaSqlStatement *stmt, GSList *list);
void gda_sql_statement_insert_take_1_values_list (GdaSqlStatement *stmt, GSList *list);
void gda_sql_statement_insert_take_extra_values_list (GdaSqlStatement *stmt, GSList *list);

void gda_sql_statement_insert_take_select (GdaSqlStatement *stmt, GdaSqlStatement *select);

G_END_DECLS

#endif
