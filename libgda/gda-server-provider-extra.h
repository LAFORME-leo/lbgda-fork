/*
 * Copyright (C) 2006 - 2009 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2007 Armin Burgmeier <armin@openismus.com>
 * Copyright (C) 2007 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
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

#ifndef __GDA_SERVER_PROVIDER_EXTRA__
#define __GDA_SERVER_PROVIDER_EXTRA__

#include <libgda/gda-decl.h>
#include <libgda/gda-value.h>
#include <libgda/gda-connection.h>

G_BEGIN_DECLS

/*
 * GdaSqlParser associated to each provider
 */
GdaSqlParser *gda_server_provider_internal_get_parser (GdaServerProvider *prov);

/*
 * Help to implement providers, so the schemas return the same
 * number of columns and column titles across the providers.
 */
gint      gda_server_provider_get_schema_nb_columns (GdaConnectionSchema schema);
gboolean  gda_server_provider_init_schema_model     (GdaDataModel *model, GdaConnectionSchema schema);
gboolean  gda_server_provider_test_schema_model     (GdaDataModel *model, GdaConnectionSchema schema, GError **error);

/*
 * Default perform operation
 */
gboolean gda_server_provider_perform_operation_default (GdaServerProvider *provider, GdaConnection *cnc,
							GdaServerOperation *op, GError **error);

/* default data handler method */
GdaDataHandler *gda_server_provider_get_data_handler_default (GdaServerProvider *provider, GdaConnection *cnc,
							      GType type, const gchar *dbms_type);

/* Convert a SELECT statement with potentially some parameters to another SELECT statement
 * where all the parameters are removed and where the WHERE condition is set to "0 = 1"
 * to make sure no row will ever be returned
 */
GdaStatement *gda_select_alter_select_for_empty (GdaStatement *stmt, GError **error);

/*
 * Help to implement the GdaDataHandler retrieving for the providers
 */
typedef struct {
	GdaConnection *cnc;
	GType          g_type;
	gchar         *dbms_type;
} GdaServerProviderHandlerInfo;

GdaDataHandler *gda_server_provider_handler_find            (GdaServerProvider *prov, GdaConnection *cnc, 
							     GType g_type, const gchar *dbms_type);
void            gda_server_provider_handler_declare         (GdaServerProvider *prov, GdaDataHandler *dh,
							     GdaConnection *cnc, 
							     GType g_type, const gchar *dbms_type);

/*
 * misc
 */
gchar         *gda_server_provider_find_file                (GdaServerProvider *prov, const gchar *inst_dir, const gchar *filename);
gchar         *gda_server_provider_load_file_contents       (const gchar *inst_dir, const gchar *data_dir, const gchar *filename);

G_END_DECLS

#endif



