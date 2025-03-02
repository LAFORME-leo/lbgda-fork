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

#ifndef __GDA_REPORT_ENGINE_H__
#define __GDA_REPORT_ENGINE_H__

#include <glib-object.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libgda/gda-statement.h>

#define GDA_TYPE_REPORT_ENGINE            (gda_report_engine_get_type())
#define GDA_REPORT_ENGINE(obj)            (G_TYPE_CHECK_INSTANCE_CAST (obj, GDA_TYPE_REPORT_ENGINE, GdaReportEngine))
#define GDA_REPORT_ENGINE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST (klass, GDA_TYPE_REPORT_ENGINE, GdaReportEngineClass))
#define GDA_IS_REPORT_ENGINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE (obj, GDA_TYPE_REPORT_ENGINE))
#define GDA_IS_REPORT_ENGINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GDA_TYPE_REPORT_ENGINE))

G_BEGIN_DECLS

typedef struct _GdaReportEngine      GdaReportEngine;
typedef struct _GdaReportEngineClass GdaReportEngineClass;
typedef struct _GdaReportEnginePrivate GdaReportEnginePrivate;

struct _GdaReportEngine {
	GObject                 base;
	GdaReportEnginePrivate *priv;
};

struct _GdaReportEngineClass {
	GObjectClass            parent_class;

	/* Padding for future expansion */
	void (*_gda_reserved1) (void);
	void (*_gda_reserved2) (void);
	void (*_gda_reserved3) (void);
	void (*_gda_reserved4) (void);
};

GType            gda_report_engine_get_type        (void) G_GNUC_CONST;

GdaReportEngine *gda_report_engine_new             (xmlNodePtr spec_node);
GdaReportEngine *gda_report_engine_new_from_string (const gchar *spec_string);
GdaReportEngine *gda_report_engine_new_from_file   (const gchar *spec_file_name);

void             gda_report_engine_declare_object  (GdaReportEngine *engine, GObject *object, const gchar *obj_name);
GObject         *gda_report_engine_find_declared_object (GdaReportEngine *engine, GType obj_type, const gchar *obj_name);

xmlNodePtr       gda_report_engine_run_as_node     (GdaReportEngine *engine, GError **error);
xmlDocPtr        gda_report_engine_run_as_doc      (GdaReportEngine *engine, GError **error);

G_END_DECLS

#endif
