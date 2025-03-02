/*
 * Copyright (C) 2007 - 2011 Vivien Malerba <malerba@gnome-db.org>
 * Copyright (C) 2008 Murray Cumming <murrayc@murrayc.com>
 * Copyright (C) 2008 Nathan <nbinont@yahoo.ca>
 * Copyright (C) 2009 Bas Driessen <bas.driessen@xobas.com>
 * Copyright (C) 2010 David King <davidk@openismus.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "gda-sql.h"
#include <virtual/libgda-virtual.h>
#include <glib/gi18n-lib.h>
#include <glib/gprintf.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include "tools-input.h"
#include "config-info.h"
#include "command-exec.h"
#include <unistd.h>
#ifdef HAVE_TERMIOS_H
#include <termios.h>
#endif
#include <sys/types.h>
#include <libgda/gda-quark-list.h>
#include <libgda/gda-meta-struct.h>
#include <libgda/gda-blob-op.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#ifndef G_OS_WIN32
#include <signal.h>
typedef void (*sighandler_t)(int);
#include <pwd.h>
#else
#include <stdlib.h>
#include <windows.h>
#endif

#ifdef HAVE_READLINE
#include <readline/readline.h>
#endif

#ifdef HAVE_LIBSOUP
#include "web-server.h"
#endif

/* options */
gboolean show_version = FALSE;

gchar *single_command = NULL;
gchar *commandsfile = NULL;
gboolean interactive = FALSE;

gboolean list_configs = FALSE;
gboolean list_providers = FALSE;

gboolean list_data_files = FALSE;
gchar *purge_data_files = NULL;

gchar *outfile = NULL;
gboolean has_threads;

#ifdef HAVE_LIBSOUP
gint http_port = -1;
gchar *auth_token = NULL;
#endif

static GOptionEntry entries[] = {
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &show_version, "Show version information and exit", NULL },	
        { "output-file", 'o', 0, G_OPTION_ARG_STRING, &outfile, "Output file", "output file"},
        { "command", 'C', 0, G_OPTION_ARG_STRING, &single_command, "Run only single command (SQL or internal) and exit", "command" },
        { "commands-file", 'f', 0, G_OPTION_ARG_STRING, &commandsfile, "Execute commands from file, then exit (except if -i specified)", "filename" },
	{ "interactive", 'i', 0, G_OPTION_ARG_NONE, &interactive, "Keep the console opened after executing a file (-f option)", NULL },
        { "list-dsn", 'l', 0, G_OPTION_ARG_NONE, &list_configs, "List configured data sources and exit", NULL },
        { "list-providers", 'L', 0, G_OPTION_ARG_NONE, &list_providers, "List installed database providers and exit", NULL },
#ifdef HAVE_LIBSOUP
	{ "http-port", 's', 0, G_OPTION_ARG_INT, &http_port, "Run embedded HTTP server on specified port", "port" },
	{ "http-token", 't', 0, G_OPTION_ARG_STRING, &auth_token, "Authentication token (required to authenticate clients)", "token phrase" },
#endif
        { "data-files-list", 0, 0, G_OPTION_ARG_NONE, &list_data_files, "List files used to hold information related to each connection", NULL },
        { "data-files-purge", 0, 0, G_OPTION_ARG_STRING, &purge_data_files, "Remove some files used to hold information related to each connection. Criteria has to be in 'all', 'non-dsn', or 'non-exist-dsn'", "criteria"},
        { NULL, 0, 0, 0, NULL, NULL, NULL }
};

/* interruption handling */
#ifndef G_OS_WIN32
struct sigaction old_sigint_handler; 
#endif
static void sigint_handler (int sig_num);
static void setup_sigint_handler (void);
typedef enum {
	SIGINT_HANDLER_DISABLED = 0,
	SIGINT_HANDLER_PARTIAL_COMMAND,
} SigintHandlerCode;
static SigintHandlerCode sigint_handler_status = SIGINT_HANDLER_DISABLED;

/* structure to hold program's data */
typedef struct {
	GSList *settings; /* list all the ConnectionSetting */
	ConnectionSetting *current; /* current connection setting to which commands are sent */
	GdaInternalCommandsList *internal_commands;

	FILE *input_stream;
	FILE *output_stream;
	gboolean output_is_pipe;
	OutputFormat output_format;

	GString *partial_command;

	GHashTable *parameters; /* key = name, value = G_TYPE_STRING GdaParameter */

#ifdef HAVE_LIBSOUP
	WebServer *server;
#endif
} MainData;
	MainData *main_data;
GString *prompt = NULL;
GMainLoop *main_loop = NULL;
gboolean exit_requested = FALSE;

static ConnectionSetting *get_current_connection_settings (SqlConsole *console);
static char   **completion_func (const char *text, int start, int end);
static void     compute_prompt (SqlConsole *console, GString *string, gboolean in_command);
static gboolean set_output_file (const gchar *file, GError **error);
static gboolean set_input_file (const gchar *file, GError **error);
static gchar   *data_model_to_string (SqlConsole *console, GdaDataModel *model);
static void     output_data_model (GdaDataModel *model);
static void     output_string (const gchar *str);
static ConnectionSetting *open_connection (SqlConsole *console, const gchar *cnc_name, const gchar *cnc_string,
					   GError **error);
static void connection_settings_free (ConnectionSetting *cs);

static gboolean treat_line_func (const gchar *cmde, gboolean *out_cmde_exec_ok);
static const char *prompt_func (void);


/* commands manipulation */
static GdaInternalCommandsList  *build_internal_commands_list (void);
static gboolean                  command_is_complete (const gchar *command);
static GdaInternalCommandResult *command_execute (SqlConsole *console,
						  const gchar *command, GError **error);

static gchar                    *result_to_string (SqlConsole *console, GdaInternalCommandResult *res);
static void                      display_result (GdaInternalCommandResult *res);

int
main (int argc, char *argv[])
{
	GOptionContext *context;
	GError *error = NULL;
	MainData *data;
	int exit_status = EXIT_SUCCESS;
	gboolean show_welcome = TRUE;
	prompt = g_string_new ("");

	context = g_option_context_new (_("[DSN|connection string]..."));        
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_ignore_unknown_options (context, TRUE);
        if (!g_option_context_parse (context, &argc, &argv, &error)) {
                g_fprintf  (stderr, "Can't parse arguments: %s\n", error->message);
		return EXIT_FAILURE;
        }
        g_option_context_free (context);

	if (show_version) {
		g_print (_("GDA SQL console version " PACKAGE_VERSION "\n"));
		return 0;
	}

	g_setenv ("GDA_CONFIG_SYNCHRONOUS", "1", TRUE);
	setlocale (LC_ALL, "");
        gda_init ();

	has_threads = g_thread_supported ();
	data = g_new0 (MainData, 1);
	data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	main_data = data;

	/* output file */
	if (outfile) {
		if (! set_output_file (outfile, &error)) {
			g_print ("Can't set output file as '%s': %s\n", outfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* treat here lists of providers and defined DSN */
	if (list_providers) {
		if (argc == 2) {
			single_command = g_strdup_printf (".lp %s", argv[1]);
			argc = 1;
			show_welcome = FALSE;
		}
		else {
			GdaDataModel *model = config_info_list_all_providers ();
			output_data_model (model);
			g_object_unref (model);
			goto cleanup;
		}
	}
	if (list_configs) {
		if (argc == 2) {
			single_command = g_strdup_printf (".l %s", argv[1]);
			argc = 1;
			show_welcome = FALSE;
		}
		else {
			GdaDataModel *model = config_info_list_all_dsn ();
			output_data_model (model);
			g_object_unref (model);
			goto cleanup;
		}
	}
	if (list_data_files) {
		gchar *confdir;
		GdaDataModel *model;

		confdir = config_info_compute_dict_directory ();
		g_print (_("All files are in the directory: %s\n"), confdir);
		g_free (confdir);
		model = config_info_list_data_files (&error);
		if (model) {
			output_data_model (model);
			g_object_unref (model);
		}
		else
			g_print (_("Can't get the list of files used to store information about each connection: %s\n"),
				 error->message);	
		goto cleanup;
	}
	if (purge_data_files) {
		gchar *tmp;
		tmp = config_info_purge_data_files (purge_data_files, &error);
		if (tmp) {
			output_string (tmp);
			g_free (tmp);
		}
		if (error)
			g_print (_("Error while purging files used to store information about each connection: %s\n"),
				 error->message);	
		goto cleanup;
	}

	/* commands file */
	if (commandsfile) {
		if (! set_input_file (commandsfile, &error)) {
			g_print ("Can't read file '%s': %s\n", commandsfile,
				 error->message);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}
	else {
		/* check if stdin is a term */
		if (!isatty (fileno (stdin))) 
			data->input_stream = stdin;
	}

	/* welcome message */
	if (show_welcome && !data->output_stream) {
#ifdef G_OS_WIN32
		HANDLE wHnd;
		SMALL_RECT windowSize = {0, 0, 139, 49};

		wHnd = GetStdHandle(STD_OUTPUT_HANDLE);

		SetConsoleTitle ("GDA SQL console, version " PACKAGE_VERSION);
		SetConsoleWindowInfo (wHnd, TRUE, &windowSize);
#endif
		g_print (_("Welcome to the GDA SQL console, version " PACKAGE_VERSION));
		g_print ("\n\n");
		g_print (_("Type: .copyright to show usage and distribution terms\n"
			   "      .? for help with internal commands\n"
			   "      .q (or CTRL-D) to quit\n"
			   "      (the '.' can be replaced by a '\\')\n"
			   "      or any query terminated by a semicolon\n\n"));
	}

	/* open connections if specified */
	gint i;
	for (i = 1; i < argc; i++) {
		/* open connection */
		ConnectionSetting *cs;
		GdaDsnInfo *info = NULL;
		gchar *str;

		if (*argv[i] == '-')
			continue;
#define SHEBANG "#!"
#define SHEBANGLEN 2

		/* try to see if argv[i] corresponds to a file starting with SHEBANG */
		FILE *file;
		file = fopen (argv[i], "r");
		if (file) {
			char buffer [SHEBANGLEN + 1];
			size_t nread;
			nread = fread (buffer, 1, SHEBANGLEN, file);
			if (nread == SHEBANGLEN) {
				buffer [SHEBANGLEN] = 0;
				if (!strcmp (buffer, SHEBANG)) {
					if (! set_input_file (argv[i], &error)) {
						g_print ("Can't read file '%s': %s\n", commandsfile,
							 error->message);
						exit_status = EXIT_FAILURE;
						fclose (file);
						goto cleanup;
					}
					fclose (file);
					continue;
				}
			}
			fclose (file);
		}

                info = gda_config_get_dsn_info (argv[i]);
		if (info)
			str = g_strdup (info->name);
		else
			str = g_strdup_printf ("c%d", i-1);
		if (!data->output_stream) 
			g_print (_("Opening connection '%s' for: %s\n"), str, argv[i]);
		cs = open_connection (NULL, str, argv[i], &error);
		config_info_modify_argv (argv[i]);
		g_free (str);
		if (!cs) {
			g_print (_("Can't open connection %d: %s\n"), i,
				 error && error->message ? error->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	if (getenv ("GDA_SQL_CNC")) {
		ConnectionSetting *cs;
		gchar *str;
		const gchar *envstr = getenv ("GDA_SQL_CNC");
		str = g_strdup_printf ("c%d", i-1);
		if (!data->output_stream) 
			g_print (_("Opening connection '%s' for: %s (GDA_SQL_CNC environment variable)\n"), 
				   str, envstr);
		cs = open_connection (NULL, str, envstr, &error);
		g_free (str);
		if (!cs) {
			g_print (_("Can't open connection defined by GDA_SQL_CNC: %s\n"),
				 error && error->message ? error->message : _("No detail"));
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}

	/* build internal command s list */
	data->internal_commands = build_internal_commands_list ();

#ifdef HAVE_LIBSOUP
	/* start HTTP server if requested */
	if (http_port > 0) {
		main_data->server = web_server_new (http_port, auth_token);
		if (!main_data->server) {
			g_print (_("Can't run HTTP server on port %d\n"), http_port);
			exit_status = EXIT_FAILURE;
			goto cleanup;
		}
	}
#endif

	/* process commands which need to be executed as specified by the command line args */
	if (single_command) {
		treat_line_func (single_command, NULL);
		if (!data->output_stream)
			g_print ("\n");
		goto cleanup;
	}

	if (data->input_stream) {
		gchar *cmde;
		for (;;) {
			cmde = input_from_stream (data->input_stream);
			if (cmde) {
				gboolean command_ok;
				treat_line_func (cmde, &command_ok);
				g_free (cmde);
				if (! command_ok)
					break;
			}
			else
				break;
		}
		if (interactive && !cmde && isatty (fileno (stdin)))
			set_input_file (NULL, NULL);
		else {
			if (!data->output_stream)
				g_print ("\n");
			goto cleanup;
		}
	}

	if (exit_requested)
		goto cleanup;

	/* set up interactive commands */
	setup_sigint_handler ();
	init_input ((TreatLineFunc) treat_line_func, prompt_func, NULL);
	set_completion_func (completion_func);
	init_history ();

	/* run main loop */
	main_loop = g_main_loop_new (NULL, TRUE);
	g_main_loop_run (main_loop);		
	g_main_loop_unref (main_loop);


 cleanup:
	/* cleanups */
	g_slist_foreach (data->settings, (GFunc) connection_settings_free, NULL);
	set_input_file (NULL, NULL); 
	set_output_file (NULL, NULL); 
	end_input ();

	g_free (data);

	return EXIT_SUCCESS;
}

static const char *
prompt_func (void)
{
	/* compute a new prompt */
	compute_prompt (NULL, prompt, main_data->partial_command == NULL ? FALSE : TRUE);
	return (char*) prompt->str;
}

/* @cmde is stolen here */
static gboolean
treat_line_func (const gchar *cmde, gboolean *out_cmde_exec_ok)
{
	gchar *loc_cmde = NULL;

	if (out_cmde_exec_ok)
		*out_cmde_exec_ok = TRUE;

	if (!cmde) {
		save_history (NULL, NULL);
		if (!main_data->output_stream)
			g_print ("\n");
		goto exit;
	}
	
	loc_cmde = g_strdup (cmde);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		add_to_history (loc_cmde);
		if (!main_data->partial_command) {
			/* enable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_PARTIAL_COMMAND;
			main_data->partial_command = g_string_new (loc_cmde);
		}
		else {
			g_string_append_c (main_data->partial_command, '\n');
			g_string_append (main_data->partial_command, loc_cmde);
		}
		if (command_is_complete (main_data->partial_command->str)) {
			/* execute command */
			GdaInternalCommandResult *res;
			FILE *to_stream;
			GError *error = NULL;
			
			if ((*main_data->partial_command->str != '\\') && (*main_data->partial_command->str != '.')) {
				if (main_data->current) {
					if (!main_data->current->query_buffer)
						main_data->current->query_buffer = g_string_new ("");
					g_string_assign (main_data->current->query_buffer, main_data->partial_command->str);
				}
			}
			
			if (main_data && main_data->output_stream)
				to_stream = main_data->output_stream;
			else
				to_stream = stdout;
			res = command_execute (NULL, main_data->partial_command->str, &error);
			
			if (!res) {
				if (!error ||
				    (error->domain != GDA_SQL_PARSER_ERROR) ||
				    (error->code != GDA_SQL_PARSER_EMPTY_SQL_ERROR)) {
					g_fprintf (to_stream,
						   "ERROR: %s\n", 
						   error && error->message ? error->message : _("No detail"));
					if (out_cmde_exec_ok)
						*out_cmde_exec_ok = FALSE;
				}
				if (error) {
					g_error_free (error);
					error = NULL;
				}
			}
			else {
				display_result (res);
				if (res->type == GDA_INTERNAL_COMMAND_RESULT_EXIT) {
					gda_internal_command_exec_result_free (res);
					exit_requested = TRUE;
					goto exit;
				}
				gda_internal_command_exec_result_free (res);
			}
			g_string_free (main_data->partial_command, TRUE);
			main_data->partial_command = NULL;
			
			/* disable SIGINT handling */
			sigint_handler_status = SIGINT_HANDLER_DISABLED;
		}
	}
	g_free (loc_cmde);
	return TRUE;

 exit:
	g_free (loc_cmde);
	if (main_loop)
		g_main_loop_quit (main_loop);
	return FALSE;
}

static void
display_result (GdaInternalCommandResult *res)
{
	switch (res->type) {
	case GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT: 
		g_print ("%s", res->u.txt->str);
		if (res->u.txt->str [strlen (res->u.txt->str) - 1] != '\n')
			g_print ("\n");
		fflush (NULL);
		break;
	case GDA_INTERNAL_COMMAND_RESULT_EMPTY:
		break;
	case GDA_INTERNAL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		for (list = res->u.multiple_results; list; list = list->next)
			display_result ((GdaInternalCommandResult *) list->data);
		break;
	}
	case GDA_INTERNAL_COMMAND_RESULT_EXIT:
		break;
	default: {
		gchar *str;
		str = result_to_string (NULL, res);
		output_string (str);
		g_free (str);
	}
	}
}

static gchar *
result_to_string (SqlConsole *console, GdaInternalCommandResult *res)
{
	OutputFormat of;
	if (console)
		of = console->output_format;
	else
		of = main_data->output_format;

	switch (res->type) {
	case GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL:
		return data_model_to_string (console, res->u.model);

	case GDA_INTERNAL_COMMAND_RESULT_SET: {
		GSList *list;
		GString *string;
		xmlNodePtr node;
		xmlBufferPtr buffer;
		gchar *str;

		switch (of) {
		case OUTPUT_FORMAT_DEFAULT:
			string = g_string_new ("");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				gchar *tmp;
				const gchar *cstr;
				GdaHolder *h;
				h = GDA_HOLDER (list->data);

				cstr = gda_holder_get_id (h);
				value = gda_holder_get_value (h);
				if (!strcmp (cstr, "IMPACTED_ROWS")) {
					g_string_append_printf (string, "%s: ",
								_("Number of rows impacted"));
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "%s", tmp);
					g_free (tmp);
				}
				else if (!strcmp (cstr, "EXEC_DELAY")) {
					g_string_append_printf (string, "%s: ",
								_("Execution delay"));
					gdouble etime;
					etime = g_value_get_double (value);
					g_string_append_printf (string, "%.03f s", etime);
				}
				else {
					tmp = g_markup_escape_text (cstr, -1);
					g_string_append_printf (string, "%s: ", tmp);
					g_free (tmp);
					
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "%s", tmp);
					g_free (tmp);
				}
				g_string_append (string, "\n");
			}
			str = string->str;
			g_string_free (string, FALSE);
			return str;

		case OUTPUT_FORMAT_XML: {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "parameters");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "parameter");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		case OUTPUT_FORMAT_HTML: {
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "ul");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				xmlNodePtr pnode, vnode;
								
				pnode = xmlNewNode (NULL, BAD_CAST "li");
				xmlAddChild (node, pnode);
				xmlSetProp (pnode, BAD_CAST "name", 
					    BAD_CAST gda_holder_get_id (GDA_HOLDER (list->data)));
				value = gda_holder_get_value (GDA_HOLDER (list->data));
				vnode = gda_value_to_xml (value);
				xmlAddChild (pnode, vnode);
			}
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		}
		case OUTPUT_FORMAT_CSV: 
			string = g_string_new ("");
			for (list = res->u.set->holders; list; list = list->next) {
				const GValue *value;
				gchar *tmp;
				const gchar *cstr;
				GdaHolder *h;
				h = GDA_HOLDER (list->data);

				cstr = gda_holder_get_id (h);
				value = gda_holder_get_value (h);
				if (!strcmp (cstr, "IMPACTED_ROWS")) {
					g_string_append_printf (string, "\"%s\",",
								_("Number of rows impacted"));
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "\"%s\"", tmp);
					g_free (tmp);
				}
				else if (!strcmp (cstr, "EXEC_DELAY")) {
					g_string_append_printf (string, "\"%s\",",
								_("Execution delay"));
					gdouble etime;
					etime = g_value_get_double (value);
					g_string_append_printf (string, "\"%.03f s\"", etime);
				}
				else {
					tmp = g_markup_escape_text (cstr, -1);
					g_string_append_printf (string, "\"%s\",", tmp);
					g_free (tmp);
					
					tmp = gda_value_stringify (value);
					g_string_append_printf (string, "\"%s\"", tmp);
					g_free (tmp);
				}
				g_string_append (string, "\n");
			}
			str = string->str;
			g_string_free (string, FALSE);
			return str;
		default:
			TO_IMPLEMENT;
			return NULL;
		}
		break;
	}
	case GDA_INTERNAL_COMMAND_RESULT_TXT: {
		xmlNodePtr node;
		xmlBufferPtr buffer;
		gchar *str;

		switch (of) {
		case OUTPUT_FORMAT_DEFAULT:
		case OUTPUT_FORMAT_CSV:
			return g_strdup (res->u.txt->str);

		case OUTPUT_FORMAT_XML: 
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "txt");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;

		case OUTPUT_FORMAT_HTML: 
			buffer = xmlBufferCreate ();
			node = xmlNewNode (NULL, BAD_CAST "p");
			xmlNodeSetContent (node, BAD_CAST res->u.txt->str);
			xmlNodeDump (buffer, NULL, node, 0, 1);
			str = g_strdup ((gchar *) xmlBufferContent (buffer));
			xmlBufferFree (buffer);
			xmlFreeNode (node);
			return str;
		default:
			TO_IMPLEMENT;
			return NULL;
		}
		break;
	}

	case GDA_INTERNAL_COMMAND_RESULT_EMPTY:
		return g_strdup ("");

	case GDA_INTERNAL_COMMAND_RESULT_MULTIPLE: {
		GSList *list;
		GString *string = NULL;
		gchar *str;

		for (list = res->u.multiple_results; list; list = list->next) {
			GdaInternalCommandResult *tres = (GdaInternalCommandResult*) list->data;
			gchar *tmp;
			
			tmp = result_to_string (console, tres);
			if (!string)
				string = g_string_new (tmp);
			else {
				g_string_append_c (string, '\n');
				g_string_append (string, tmp);
			}
			g_free (tmp);
		}
		if (string) {
			str = string->str;
			g_string_free (string, FALSE);
		}
		else
			str = g_strdup ("");
		return str;
	}
	
	default:
		return NULL;
	}
}

/*
 * SIGINT handling
 */

static void 
setup_sigint_handler (void) 
{
#ifndef G_OS_WIN32
	struct sigaction sac;
	memset (&sac, 0, sizeof (sac));
	sigemptyset (&sac.sa_mask);
	sac.sa_handler = sigint_handler;
	sac.sa_flags = SA_RESTART;
	sigaction (SIGINT, &sac, &old_sigint_handler);
#endif
}

#ifndef G_OS_WIN32
static void
sigint_handler (int sig_num)
{
	if (sigint_handler_status == SIGINT_HANDLER_PARTIAL_COMMAND) {
		if (main_data->partial_command) {
			g_string_free (main_data->partial_command, TRUE);
			main_data->partial_command = NULL;
		}
		/* show a new prompt */
		compute_prompt (NULL, prompt, main_data->partial_command == NULL ? FALSE : TRUE);
		g_print ("\n%s", prompt->str);
		fflush (NULL);
	}
	else {
		g_print ("\n");
		exit (EXIT_SUCCESS);
	}

	if (old_sigint_handler.sa_handler)
		old_sigint_handler.sa_handler (sig_num);
}
#endif

/*
 * command_is_complete
 *
 * Checks if @command can be executed, or if more data is required
 */
static gboolean
command_is_complete (const gchar *command)
{
	if (!command || !(*command))
		return FALSE;
	if (single_command)
		return TRUE;
	if ((*command == '\\') || (*command == '.')) {
		/* internal command */
		return TRUE;
	}
	else if (*command == '#') {
		/* comment, to be ignored */
		return TRUE;
	}
	else {
		gint i, len;
		len = strlen (command);
		for (i = len - 1; i > 0; i--) {
			if ((command [i] != ' ') && (command [i] != '\n') && (command [i] != '\r'))
				break;
		}
		if (command [i] == ';')
			return TRUE;
		else
			return FALSE;
	}
}


/*
 * command_execute
 */
static GdaInternalCommandResult *execute_internal_command (SqlConsole *console, GdaConnection *cnc,
							   const gchar *command_str, GError **error);
static GdaInternalCommandResult *execute_external_command (SqlConsole *console, const gchar *command, GError **error);
static GdaInternalCommandResult *
command_execute (SqlConsole *console, const gchar *command, GError **error)
{
	ConnectionSetting *cs;

	cs = get_current_connection_settings (console);
	if (!command || !(*command))
                return NULL;
        if ((*command == '\\') || (*command == '.')) {
                if (cs)
                        return execute_internal_command (console, cs->cnc, command, error);
                else
                        return execute_internal_command (console, NULL, command, error);
        }
	else if (*command == '#') {
		/* nothing to do */
		GdaInternalCommandResult *res;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
        else {
                if (!cs) {
                        g_set_error (error, 0, 0, "%s", 
                                     _("No connection specified"));
                        return NULL;
                }
                if (!gda_connection_is_opened (cs->cnc)) {
                        g_set_error (error, 0, 0, "%s", 
                                     _("Connection closed"));
                        return NULL;
                }

                return execute_external_command (console, command, error);
        }
}

static gint
commands_compare_name (GdaInternalCommand *a, GdaInternalCommand *b)
{
	gint cmp, alength, blength;
	if (!a->name || !b->name) {
		g_warning (_("Invalid unnamed command"));
		if (!a->name) {
			if (b->name)
				return 1;
			else
				return 0;
		}
		else
			return -1;
	}
	alength = strlen (a->name);
	blength = strlen (b->name);
	cmp = strncmp (a->name, b->name, MIN (alength, blength));
	if (cmp == 0) 
		return blength - alength;
	else
		return cmp;
}


static gint
commands_compare_group (GdaInternalCommand *a, GdaInternalCommand *b)
{
	if (!a->group) {
		if (b->group)
			return 1;
		else
			return 0;
	}
	else {
		if (b->group) {
			gint cmp = strcmp (a->group, b->group);
			if (cmp)
				return cmp;
			else 
				return commands_compare_name (a, b);
		}
		else
			return -1;
	}
}


static GdaInternalCommand *
find_command (GdaInternalCommandsList *commands_list, const gchar *command_str, gboolean *command_complete)
{
	GdaInternalCommand *command = NULL;
	GSList *list;
	gsize length;

	if (!command_str || ((*command_str != '\\') && (*command_str != '.')))
		return NULL;

	length = strlen (command_str + 1);
	for (list = commands_list->name_ordered; list; list = list->next) {
		command = (GdaInternalCommand*) list->data;
		if (!strncmp (command->name, command_str + 1, MIN (length, strlen (command->name)))) {
			gsize l;
			gchar *ptr;
			for (ptr = command->name, l = 0; *ptr && (*ptr != ' '); ptr++, l++);
				
			if (length == l)
				break;
			else
				command = NULL;
		}
		else
			command = NULL;
	}

	/* FIXME */
	if (command_complete)
		*command_complete = TRUE;

	return command;
}


/*
 * execute_internal_command
 *
 * Executes an internal command (not SQL)
 */
GdaInternalCommandResult *
execute_internal_command (SqlConsole *console, GdaConnection *cnc, const gchar *command_str, GError **error)
{
	GdaInternalCommand *command;
	gboolean command_complete;
	gchar **args;
	GdaInternalCommandResult *res = NULL;
	GdaInternalCommandsList *commands_list = main_data->internal_commands;

	if (!commands_list->name_ordered) {
		GSList *list;

		for (list = commands_list->commands; list; list = list->next) {
			commands_list->name_ordered = 
				g_slist_insert_sorted (commands_list->name_ordered, list->data,
						       (GCompareFunc) commands_compare_name);
			commands_list->group_ordered = 
				g_slist_insert_sorted (commands_list->group_ordered, list->data,
						       (GCompareFunc) commands_compare_group);
		}
	}

	args = g_strsplit (command_str, " ", 2);
	command = find_command (commands_list, args[0], &command_complete);
	g_strfreev (args);
	args = NULL;
	if (!command) {
		g_set_error (error, 0, 0, "%s", 
			     _("Unknown internal command"));
		goto cleanup;
	}

	if (!command->command_func) {
		g_set_error (error, 0, 0, "%s", 
			     _("Internal command not correctly defined"));
		goto cleanup;
	}

	if (!command_complete) {
		g_set_error (error, 0, 0, "%s", 
			     _("Incomplete internal command"));
		goto cleanup;
	}

	if (command->arguments_delimiter_func)
		args = command->arguments_delimiter_func (command_str);
	else
		args = default_gda_internal_commandargs_func (command_str);
	if (command->unquote_args) {
		gint i;
		for (i = 1; args[i]; i++) 
			gda_internal_command_arg_remove_quotes (args[i]);
	}
	res = command->command_func (console, cnc, (const gchar **) &(args[1]), 
				     error, command->user_data);
	
 cleanup:
	if (args)
		g_strfreev (args);

	return res;
}


/*
 * execute_external_command
 *
 * Executes an SQL statement as understood by the DBMS
 */
static GdaInternalCommandResult *
execute_external_command (SqlConsole *console, const gchar *command, GError **error)
{
	GdaInternalCommandResult *res = NULL;
	GdaBatch *batch;
	const GSList *stmt_list;
	GdaStatement *stmt;
	GdaSet *params;
	GObject *obj;
	const gchar *remain = NULL;
	ConnectionSetting *cs;

	cs = get_current_connection_settings (console);
	if (!cs) {
		g_set_error (error, 0, 0, "%s", 
			     _("No connection specified"));
		return NULL;
	}

	batch = gda_sql_parser_parse_string_as_batch (cs->parser, command, &remain, error);
	if (!batch)
		return NULL;
	if (remain) {
		g_object_unref (batch);
		return NULL;
	}
	
	stmt_list = gda_batch_get_statements (batch);
	if (!stmt_list) {
		g_object_unref (batch);
		return NULL;
	}

	if (stmt_list->next) {
		g_set_error (error, 0, 0, "%s", 
			     _("More than one SQL statement"));
		g_object_unref (batch);
		return NULL;
	}
		
	stmt = GDA_STATEMENT (stmt_list->data);
	g_object_ref (stmt);
	g_object_unref (batch);

	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	/* fill parameters with some defined parameters */
	if (params && params->holders) {
		GSList *list;
		for (list = params->holders; list; list = list->next) {
			GdaHolder *h = GDA_HOLDER (list->data);
			GdaHolder *h_in_data = g_hash_table_lookup (main_data->parameters, gda_holder_get_id (h));
			if (h_in_data) {
				const GValue *cvalue;
				GValue *value;

				cvalue = gda_holder_get_value (h_in_data);
				if (cvalue && (G_VALUE_TYPE (cvalue) == gda_holder_get_g_type (h))) {
					if (!gda_holder_set_value (h, cvalue, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
				else if (cvalue && (G_VALUE_TYPE (cvalue) == GDA_TYPE_NULL)) {
					if (!gda_holder_set_value (h, cvalue, error)) {
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
				else {
					gchar *str;
					str = gda_value_stringify (cvalue);
					value = gda_value_new_from_string (str, gda_holder_get_g_type (h));
					g_free (str);
					if (! value) {
						g_set_error (error, 0, 0,
							     _("Could not interpret the '%s' parameter's value"), 
							     gda_holder_get_id (h));
						g_free (res);
						res = NULL;
						goto cleanup;
					}
					else if (! gda_holder_take_value (h, value, error)) {
						gda_value_free (value);
						g_free (res);
						res = NULL;
						goto cleanup;
					}
				}
			}
			else {
				if (! gda_holder_is_valid (h)) {
					g_set_error (error, 0, 0,
						     _("No internal parameter named '%s' required by query"), 
						     gda_holder_get_id (h));
					g_free (res);
					res = NULL;
					goto cleanup;
				}
			}
		}
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->was_in_transaction_before_exec = gda_connection_get_transaction_status (cs->cnc) ? TRUE : FALSE;
	res->cnc = g_object_ref (cs->cnc);
	obj = gda_connection_statement_execute (cs->cnc, stmt, params, 
						GDA_STATEMENT_MODEL_RANDOM_ACCESS, NULL, error);
	if (!obj) {
		g_free (res);
		res = NULL;
	}
	else {
		if (GDA_IS_DATA_MODEL (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = GDA_DATA_MODEL (obj);
		}
		else if (GDA_IS_SET (obj)) {
			res->type = GDA_INTERNAL_COMMAND_RESULT_SET;
			res->u.set = GDA_SET (obj);
		}
		else
			g_assert_not_reached ();
	}

 cleanup:
	g_object_unref (stmt);
	if (params)
		g_object_unref (params);

	return res;
}

static ConnectionSetting *
get_current_connection_settings (SqlConsole *console)
{
	if (console) {
		if (console->current) {
			if (g_slist_find (main_data->settings, console->current))
				return console->current;
			else
				return NULL;
		}
		else
			return NULL;
	}
	else
		return main_data->current;
}

static void
compute_prompt (SqlConsole *console, GString *string, gboolean in_command)
{
	gchar *prefix = NULL;
	ConnectionSetting *cs;
	g_assert (string);
	gchar suffix = '>';

	g_string_set_size (string, 0);
	cs = get_current_connection_settings (console);
	if (cs) {
		prefix = cs->name;
		if (cs->cnc) {
			GdaTransactionStatus *ts;
			ts = gda_connection_get_transaction_status (cs->cnc);
			if (ts)
				suffix='[';
		}
	}
	else
		prefix = "gda";

	if (in_command) {
		gint i, len;
		len = strlen (prefix);
		for (i = 0; i < len; i++)
			g_string_append_c (string, ' ');
		g_string_append_c (string, suffix);
		g_string_append_c (string, ' ');		
	}
	else 
		g_string_append_printf (string, "%s%c ", prefix, suffix);
}

/*
 * Change the output file, set to %NULL to be back on stdout
 */
static gboolean
set_output_file (const gchar *file, GError **error)
{
	if (main_data->output_stream) {
		if (main_data->output_is_pipe) {
			pclose (main_data->output_stream);
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_DFL);
#endif
		}
		else
			fclose (main_data->output_stream);
		main_data->output_stream = NULL;
		main_data->output_is_pipe = FALSE;
	}

	if (file) {
		gchar *copy = g_strdup (file);
		g_strchug (copy);

		if (*copy != '|') {
			/* output to a file */
			main_data->output_stream = g_fopen (copy, "w");
			if (!main_data->output_stream) {
				g_set_error (error, 0, 0,
					     _("Can't open file '%s' for writing: %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
			main_data->output_is_pipe = FALSE;
		}
		else {
			/* output to a pipe */
			main_data->output_stream = popen (copy+1, "w");
			if (!main_data->output_stream) {
				g_set_error (error, 0, 0,
					     _("Can't open pipe '%s': %s\n"), 
					     copy,
					     strerror (errno));
				g_free (copy);
				return FALSE;
			}
#ifndef G_OS_WIN32
			signal (SIGPIPE, SIG_IGN);
#endif
			main_data->output_is_pipe = TRUE;
		}
		g_free (copy);
	}

	return TRUE;
}

/*
 * Change the input file, set to %NULL to be back on stdin
 */
static gboolean
set_input_file (const gchar *file, GError **error)
{
	if (main_data->input_stream) {
		fclose (main_data->input_stream);
		main_data->input_stream = NULL;
	}

	if (file) {
		if (*file == '~') {
			gchar *tmp;
			tmp = g_strdup_printf ("%s%s", g_get_home_dir (), file+1);
			main_data->input_stream = g_fopen (tmp, "r");
			g_free (tmp);
		}
		else
			main_data->input_stream = g_fopen (file, "r");
		if (!main_data->input_stream) {
			g_set_error (error, 0, 0,
				     _("Can't open file '%s' for reading: %s\n"), 
				     file,
				     strerror (errno));
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
connection_name_is_valid (const gchar *name)
{
	const gchar *ptr;
	if (!name || !*name)
		return FALSE;
	if (! g_ascii_isalpha (*name) && (*name != '_'))
		return FALSE;
	for (ptr = name; *ptr; ptr++)
		if (!g_ascii_isalnum (*ptr) && (*ptr != '_'))
			return FALSE;
	return TRUE;
}

static ConnectionSetting *
find_connection_from_name (const gchar *name)
{
	ConnectionSetting *cs = NULL;
	GSList *list;
	for (list = main_data->settings; list; list = list->next) {
		if (!strcmp (name, ((ConnectionSetting *) list->data)->name)) {
			cs = (ConnectionSetting *) list->data;
			break;
		}
	}
	return cs;
}

typedef struct {
	ConnectionSetting *cs;
	gboolean cannot_lock;
	gboolean result;
	GError   *error;
} MetaUpdateData;

static gpointer thread_start_update_meta_store (MetaUpdateData *data);
static void thread_ok_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);
static void thread_cancelled_cb_update_meta_store (GdaThreader *threader, guint job, MetaUpdateData *data);
static void conn_closed_cb (GdaConnection *cnc, ConnectionSetting *cs);

static gchar* read_hidden_passwd ();
static void user_password_needed (GdaDsnInfo *info, const gchar *real_provider,
				  gboolean *out_need_username, gboolean *out_need_password,
				  gboolean *out_need_password_if_user);

/*
 * Open a connection
 */
static ConnectionSetting*
open_connection (SqlConsole *console, const gchar *cnc_name, const gchar *cnc_string, GError **error)
{
	GdaConnection *newcnc = NULL;
	ConnectionSetting *cs = NULL;
	gchar *real_cnc_string;

	if (cnc_name && ! connection_name_is_valid (cnc_name)) {
		g_set_error (error, 0, 0,
			     _("Connection name '%s' is invalid"), cnc_name);
		return NULL;
	}
	
	GdaDsnInfo *info;
	gboolean need_user, need_pass, need_password_if_user;
	gchar *user, *pass, *real_cnc, *real_provider, *real_auth_string = NULL;

	/* if cnc string is a regular file, then use it with SQLite */
	if (g_file_test (cnc_string, G_FILE_TEST_IS_REGULAR)) {
		gchar *path, *file, *e1, *e2;
		const gchar *pname = "SQLite";
		
		path = g_path_get_dirname (cnc_string);
		file = g_path_get_basename (cnc_string);
		if (g_str_has_suffix (file, ".mdb")) {
			pname = "MSAccess";
			file [strlen (file) - 4] = 0;
		}
		else if (g_str_has_suffix (file, ".db"))
			file [strlen (file) - 3] = 0;
		e1 = gda_rfc1738_encode (path);
		e2 = gda_rfc1738_encode (file);
		g_free (path);
		g_free (file);
		real_cnc_string = g_strdup_printf ("%s://DB_DIR=%s;EXTRA_FUNCTIONS=TRUE;DB_NAME=%s", pname, e1, e2);
		g_free (e1);
		g_free (e2);
		gda_connection_string_split (real_cnc_string, &real_cnc, &real_provider, &user, &pass);
	}
	else {
		gda_connection_string_split (cnc_string, &real_cnc, &real_provider, &user, &pass);
		real_cnc_string = g_strdup (cnc_string);
	}

	info = gda_config_get_dsn_info (real_cnc);
	user_password_needed (info, real_provider, &need_user, &need_pass, &need_password_if_user);

	if (!real_cnc) {
		g_free (user);
		g_free (pass);
		g_free (real_provider);
		g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
			     _("Malformed connection string '%s'"), cnc_string);
		g_free (real_cnc_string);
		return NULL;
	}

	if ((!user || !pass) && info && info->auth_string) {
		GdaQuarkList* ql;
		const gchar *tmp;
		ql = gda_quark_list_new_from_string (info->auth_string);
		if (!user) {
			tmp = gda_quark_list_find (ql, "USERNAME");
			if (tmp)
				user = g_strdup (tmp);
		}
		if (!pass) {
			tmp = gda_quark_list_find (ql, "PASSWORD");
			if (tmp)
				pass = g_strdup (tmp);
		}
		gda_quark_list_free (ql);
	}
	if (need_user && ((user && !*user) || !user)) {
		gchar buf[80], *ptr;
		g_print (_("\tUsername for '%s': "), cnc_name);
		if (! fgets (buf, 80, stdin)) {
			g_free (real_cnc);
			g_free (user);
			g_free (pass);
			g_free (real_provider);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
				     _("No username for '%s'"), cnc_string);
			g_free (real_cnc_string);
			return NULL;
		}
		for (ptr = buf; *ptr; ptr++) {
			if (*ptr == '\n') {
				*ptr = 0;
				break;
			}
		}
		g_free (user);
		user = g_strdup (buf);
	}
	if (user)
		need_pass = need_password_if_user;
	if (need_pass && ((pass && !*pass) || !pass)) {
		gchar *tmp;
		g_print (_("\tPassword for '%s': "), cnc_name);
		tmp = read_hidden_passwd ();
		g_print ("\n");
		if (! tmp) {
			g_free (real_cnc);
			g_free (user);
			g_free (pass);
			g_free (real_provider);
			g_set_error (error, GDA_CONNECTION_ERROR, GDA_CONNECTION_DSN_NOT_FOUND_ERROR, 
				     _("No password for '%s'"), cnc_string);
			g_free (real_cnc_string);
			return NULL;
		}
		g_free (pass);
		pass = tmp;
	}

	if (user || pass) {
		GString *string;
		string = g_string_new ("");
		if (user) {
			gchar *enc;
			enc = gda_rfc1738_encode (user);
			g_string_append_printf (string, "USERNAME=%s", enc);
			g_free (enc);
		}
		if (pass) {
			gchar *enc;
			enc = gda_rfc1738_encode (pass);
			if (user)
				g_string_append_c (string, ';');
			g_string_append_printf (string, "PASSWORD=%s", enc);
			g_free (enc);
		}
		real_auth_string = g_string_free (string, FALSE);
	}
	
	if (info && !real_provider)
		newcnc = gda_connection_open_from_dsn (real_cnc_string, real_auth_string,
						       GDA_CONNECTION_OPTIONS_THREAD_SAFE |
						       GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);
	else 
		newcnc = gda_connection_open_from_string (NULL, real_cnc_string, real_auth_string,
							  GDA_CONNECTION_OPTIONS_THREAD_SAFE |
							  GDA_CONNECTION_OPTIONS_AUTO_META_DATA, error);

	g_free (real_cnc_string);
	g_free (real_cnc);
	g_free (user);
	g_free (pass);
	g_free (real_provider);
	g_free (real_auth_string);

	if (newcnc) {
		gchar *dict_file_name = NULL;
		gchar *cnc_string;
		const gchar *rootname;
		gint i;
		g_object_set (G_OBJECT (newcnc), "execution-timer", TRUE, NULL);
		g_object_get (G_OBJECT (newcnc),
			      "cnc-string", &cnc_string, NULL);
		dict_file_name = config_info_compute_dict_file_name (info, cnc_string);
		g_free (cnc_string);

		cs = g_new0 (ConnectionSetting, 1);
		if (cnc_name && *cnc_name)
			rootname = cnc_name;
		else
			rootname = "c";
		if (gda_sql_get_connection (rootname)) {
			for (i = 0; ; i++) {
				gchar *tmp;
				tmp = g_strdup_printf ("%s%d", rootname, i);
				if (gda_sql_get_connection (tmp))
					g_free (tmp);
				else {
					cs->name = tmp;
					break;
				}
			}
		}
		else
			cs->name = g_strdup (rootname);
		cs->parser = gda_connection_create_parser (newcnc);
		if (!cs->parser)
			cs->parser = gda_sql_parser_new ();
		cs->cnc = newcnc;
		cs->query_buffer = NULL;
		cs->threader = NULL;
		cs->meta_job_id = 0;

		main_data->settings = g_slist_append (main_data->settings, cs);
		if (console)
			console->current = cs;
		else
			main_data->current = cs;
		
		GdaMetaStore *store;
		gboolean update_store = FALSE;

		if (dict_file_name) {
			if (! g_file_test (dict_file_name, G_FILE_TEST_EXISTS))
				update_store = TRUE;
			store = gda_meta_store_new_with_file (dict_file_name);
			g_print (_("All the information related to the '%s' connection will be stored "
				   "in the '%s' file\n"),
				 cs->name, dict_file_name);
		}
		else {
			store = gda_meta_store_new (NULL);
			if (store)
				update_store = TRUE;
		}

		config_info_update_meta_store_properties (store, newcnc);

		g_object_set (G_OBJECT (cs->cnc), "meta-store", store, NULL);
		if (update_store) {
			GError *lerror = NULL;
			
			if (has_threads) {
				MetaUpdateData *thdata;
				cs->threader = (GdaThreader*) gda_threader_new ();
				thdata = g_new0 (MetaUpdateData, 1);
				thdata->cs = cs;
				thdata->cannot_lock = FALSE;
				cs->meta_job_id = gda_threader_start_thread (cs->threader, 
									     (GThreadFunc) thread_start_update_meta_store,
									     thdata,
									     (GdaThreaderFunc) thread_ok_cb_update_meta_store,
									     (GdaThreaderFunc) thread_cancelled_cb_update_meta_store, 
									     &lerror);
				if (cs->meta_job_id == 0) {
					if (!main_data->output_stream) 
						g_print (_("Error getting meta data in background: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
			}
			else {
				if (!main_data->output_stream) {
					g_print (_("Getting database schema information for connection '%s', this may take some time... "),
						 cs->name);
					fflush (stdout);
				}
				
				if (!gda_connection_update_meta_store (cs->cnc, NULL, &lerror)) {
					if (!main_data->output_stream) 
						g_print (_("error: %s\n"), 
							 lerror && lerror->message ? lerror->message : _("No detail"));
					if (lerror)
						g_error_free (lerror);
				}
				else
					if (!main_data->output_stream) 
						g_print (_("Done.\n"));
			}
		}
		if (store)
			g_object_unref (store);
		g_free (dict_file_name);
	}

	if (cs) {
		g_signal_connect (cs->cnc, "conn-closed",
				  G_CALLBACK (conn_closed_cb), cs);
	}

	return cs;
}

static void
user_password_needed (GdaDsnInfo *info, const gchar *real_provider,
		      gboolean *out_need_username, gboolean *out_need_password,
		      gboolean *out_need_password_if_user)
{
	GdaProviderInfo *pinfo = NULL;
	if (out_need_username)
		*out_need_username = FALSE;
	if (out_need_password)
		*out_need_password = FALSE;
	if (out_need_password_if_user)
		*out_need_password_if_user = FALSE;

	if (real_provider)
		pinfo = gda_config_get_provider_info (real_provider);
	else if (info)
		pinfo = gda_config_get_provider_info (info->provider);

	if (pinfo && pinfo->auth_params) {
		if (out_need_password) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "PASSWORD");
			if (h && gda_holder_get_not_null (h))
				*out_need_password = TRUE;
		}
		if (out_need_username) {
			GdaHolder *h;
			h = gda_set_get_holder (pinfo->auth_params, "USERNAME");
			if (h && gda_holder_get_not_null (h))
				*out_need_username = TRUE;
		}
		if (out_need_password_if_user) {
			if (gda_set_get_holder (pinfo->auth_params, "PASSWORD") &&
			    gda_set_get_holder (pinfo->auth_params, "USERNAME"))
				*out_need_password_if_user = TRUE;
		}
	}
}

static gchar*
read_hidden_passwd (void)
{
	gchar *p, password [100];

#ifdef HAVE_TERMIOS_H
	int fail;
	struct termios termio;
	
	fail = tcgetattr (0, &termio);
	if (fail)
		return NULL;
	
	termio.c_lflag &= ~ECHO;
	fail = tcsetattr (0, TCSANOW, &termio);
	if (fail)
		return NULL;
#else
  #ifdef G_OS_WIN32
	HANDLE  t = NULL;
        LPDWORD t_orig = NULL;

	/* get a new handle to turn echo off */
	t_orig = (LPDWORD) malloc (sizeof (DWORD));
	t = GetStdHandle (STD_INPUT_HANDLE);
	
	/* save the old configuration first */
	GetConsoleMode (t, t_orig);
	
	/* set to the new mode */
	SetConsoleMode (t, ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT);
  #endif
#endif
	
	p = fgets (password, sizeof (password) - 1, stdin);

#ifdef HAVE_TERMIOS_H
	termio.c_lflag |= ECHO;
	tcsetattr (0, TCSANOW, &termio);
#else
  #ifdef G_OS_WIN32
	SetConsoleMode (t, *t_orig);
	fflush (stdout);
	free (t_orig);
  #endif
#endif
	
	if (!p)
		return NULL;

	for (p = password; *p; p++) {
		if (*p == '\n') {
			*p = 0;
			break;
		}
	}
	p = g_strdup (password);
	memset (password, 0, sizeof (password));

	return p;
}

static gpointer
thread_start_update_meta_store (MetaUpdateData *data)
{
	/* test if the connection can be locked, which means that multiple threads can access it at the same time.
	 * If that is not possible, then quit the thread while positionning data->cannot_lock to TRUE so the
	 * meta data update can be done while back in the main thread */
	if (gda_lockable_trylock (GDA_LOCKABLE (data->cs->cnc))) {
		gda_lockable_unlock (GDA_LOCKABLE (data->cs->cnc));
		data->result = gda_connection_update_meta_store (data->cs->cnc, NULL, &(data->error));
	}
	else
		data->cannot_lock = TRUE;
	return NULL;
}

static void
thread_ok_cb_update_meta_store (G_GNUC_UNUSED GdaThreader *threader, G_GNUC_UNUSED guint job, MetaUpdateData *data)
{
	data->cs->meta_job_id = 0;
	if (data->cannot_lock) {
		GError *lerror = NULL;
		if (!main_data->output_stream) {
			g_print (_("Getting database schema information for connection '%s', this may take some time... "),
				 data->cs->name);
			fflush (stdout);
		}

		if (!gda_connection_update_meta_store (data->cs->cnc, NULL, &lerror)) {
			if (!main_data->output_stream) 
				g_print (_("error: %s\n"), 
					 lerror && lerror->message ? lerror->message : _("No detail"));
			if (lerror)
				g_error_free (lerror);
		}
		else
			if (!main_data->output_stream) 
				g_print (_("Done.\n"));
	}
	if (data->error)
		g_error_free (data->error);

	g_free (data);
}

static void
thread_cancelled_cb_update_meta_store (G_GNUC_UNUSED GdaThreader *threader, G_GNUC_UNUSED guint job,
				       MetaUpdateData *data)
{
	data->cs->meta_job_id = 0;
	if (data->error)
		g_error_free (data->error);
	g_free (data);
}

/* free the connection settings */
static void
connection_settings_free (ConnectionSetting *cs)
{
	g_free (cs->name);
	if (cs->cnc) {
		g_signal_handlers_disconnect_by_func (cs->cnc,
						      G_CALLBACK (conn_closed_cb), cs);
		g_object_unref (cs->cnc);
	}
	if (cs->parser)
		g_object_unref (cs->parser);
	if (cs->query_buffer)
		g_string_free (cs->query_buffer, TRUE);
	if (cs->threader) {
		if (cs->meta_job_id)
			gda_threader_cancel (cs->threader, cs->meta_job_id);
		g_object_unref (cs->threader);
	}
	g_free (cs);
}

/* 
 * Dumps the data model contents onto @data->output
 */
static void
output_data_model (GdaDataModel *model)
{
	gchar *str;
	str = data_model_to_string (NULL, model);
	output_string (str);
	g_free (str);
}

static gchar *
data_model_to_string (SqlConsole *console, GdaDataModel *model)
{
	static gboolean env_set = FALSE;
	OutputFormat of;

	if (!env_set) {
		if (! getenv ("GDA_DATA_MODEL_DUMP_TITLE"))
			g_setenv ("GDA_DATA_MODEL_DUMP_TITLE", "Yes", TRUE);
		if (! getenv ("GDA_DATA_MODEL_NULL_AS_EMPTY"))
			g_setenv ("GDA_DATA_MODEL_NULL_AS_EMPTY", "Yes", TRUE);
		if (! main_data->output_stream || isatty (fileno (main_data->output_stream))) {
			if (! getenv ("GDA_DATA_MODEL_DUMP_TRUNCATE"))
				g_setenv ("GDA_DATA_MODEL_DUMP_TRUNCATE", "-1", TRUE);
		}
		env_set = TRUE;
	}
	
	if (console)
		of = console->output_format;
	else
		of = main_data->output_format;

	switch (of) {
	case OUTPUT_FORMAT_DEFAULT: {
		gchar *tmp;
		tmp = gda_data_model_dump_as_string (model);
		if (GDA_IS_DATA_SELECT (model)) {
			gchar *tmp2, *tmp3;
			gdouble etime;
			g_object_get ((GObject*) model, "execution-delay", &etime, NULL);
			tmp2 = g_strdup_printf (_("Execution delay: %.03f"), etime);
			tmp3 = g_strdup_printf ("%s\n%s", tmp, tmp2);
			g_free (tmp);
			g_free (tmp2);
			return tmp3;
		}
		else
			return tmp;
		break;
	}
	case OUTPUT_FORMAT_XML:
		return gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_DATA_ARRAY_XML,
							NULL, 0,
							NULL, 0, NULL);
		break;
	case OUTPUT_FORMAT_CSV:
		return gda_data_model_export_to_string (model, GDA_DATA_MODEL_IO_TEXT_SEPARATED,
							NULL, 0,
							NULL, 0, NULL);
		break;
	case OUTPUT_FORMAT_HTML: {
		xmlBufferPtr buffer;
		xmlNodePtr top, div, table, node, row_node, col_node, header, meta;
		gint ncols, nrows, i, j;
		gchar *str;

		top = xmlNewNode (NULL, BAD_CAST "html");
		header = xmlNewChild (top, NULL, BAD_CAST "head", NULL);
		meta = xmlNewChild (header, NULL, BAD_CAST "meta", NULL);
		xmlSetProp (meta, BAD_CAST "http-equiv", BAD_CAST "content-type");
		xmlSetProp (meta, BAD_CAST "content", BAD_CAST "text/html; charset=UTF-8");
		div = xmlNewChild (top, NULL, BAD_CAST "body", NULL);
		table = xmlNewChild (div, NULL, BAD_CAST "table", NULL);
		xmlSetProp (table, BAD_CAST "border", BAD_CAST "1");
		
		if (g_object_get_data (G_OBJECT (model), "name"))
			xmlNewTextChild (table, NULL, BAD_CAST "caption", g_object_get_data (G_OBJECT (model), "name"));

		ncols = gda_data_model_get_n_columns (model);
		nrows = gda_data_model_get_n_rows (model);

		row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
		for (j = 0; j < ncols; j++) {
			const gchar *cstr;
			cstr = gda_data_model_get_column_title (model, j);
			col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "th", BAD_CAST cstr);
			xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "center");
		}

		for (i = 0; i < nrows; i++) {
			row_node = xmlNewChild (table, NULL, BAD_CAST "tr", NULL);
			xmlSetProp (row_node, BAD_CAST "valign", BAD_CAST "top");
			for (j = 0; j < ncols; j++) {
				const GValue *value;
				value = gda_data_model_get_value_at (model, j, i, NULL);
				if (!value) {
					col_node = xmlNewChild (row_node, NULL, BAD_CAST "td", BAD_CAST "ERROR");
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
				}
				else {
					str = gda_value_stringify (value);
					col_node = xmlNewTextChild (row_node, NULL, BAD_CAST "td", BAD_CAST str);
					xmlSetProp (col_node, BAD_CAST "align", BAD_CAST "left");
					g_free (str);
				}
			}
		}

		node = xmlNewChild (div, NULL, BAD_CAST "p", NULL);
		str = g_strdup_printf (ngettext ("(%d row)", "(%d rows)", nrows), nrows);
		xmlNodeSetContent (node, BAD_CAST str);
		g_free (str);

		buffer = xmlBufferCreate ();
		xmlNodeDump (buffer, NULL, top, 0, 1);
		str = g_strdup ((gchar *) xmlBufferContent (buffer));
		xmlBufferFree (buffer);
		xmlFreeNode (top);
		return str;
	}
	default: 
		TO_IMPLEMENT;
		break;
	}
	return NULL;
}

static void
output_string (const gchar *str)
{
	FILE *to_stream;
	gboolean append_nl = FALSE;
	gint length;
	static gint force_no_pager = -1;

	if (!str)
		return;

	if (force_no_pager < 0) {
		/* still unset... */
		if (getenv ("GDA_NO_PAGER"))
			force_no_pager = 1;
		else
			force_no_pager = 0;	
	}

	length = strlen (str);
	if (*str && (str[length - 1] != '\n'))
		append_nl = TRUE;

	if (main_data->output_stream)
		to_stream = main_data->output_stream;
	else
		to_stream = stdout;

	if (!force_no_pager && isatty (fileno (to_stream))) {
		/* use pager */
		FILE *pipe;
		const char *pager;
#ifndef G_OS_WIN32
		sighandler_t phandler;
#endif
		pager = getenv ("PAGER");
		if (!pager)
			pager = "more";
		pipe = popen (pager, "w");
#ifndef G_OS_WIN32
		phandler = signal (SIGPIPE, SIG_IGN);
#endif
		if (append_nl)
			g_fprintf (pipe, "%s\n", str);
		else
			g_fprintf (pipe, "%s", str);
		pclose (pipe);
#ifndef G_OS_WIN32
		signal (SIGPIPE, phandler);
#endif
	}
	else {
		if (append_nl)
			g_fprintf (to_stream, "%s\n", str);
		else
			g_fprintf (to_stream, "%s", str);
	}
}

static gchar **args_as_string_func (const gchar *str);
static gchar **args_as_string_set (const gchar *str);

static GdaInternalCommandResult *extra_command_copyright (SqlConsole *console, GdaConnection *cnc,
							  const gchar **args,
							  GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_quit (SqlConsole *console, GdaConnection *cnc,
						     const gchar **args,
						     GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_cd (SqlConsole *console, GdaConnection *cnc,
						   const gchar **args,
						   GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_set_output (SqlConsole *console, GdaConnection *cnc,
							   const gchar **args,
							   GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_set_output_format (SqlConsole *console, GdaConnection *cnc,
								  const gchar **args,
								  GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_set_input (SqlConsole *console, GdaConnection *cnc,
							  const gchar **args,
							  GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_echo (SqlConsole *console, GdaConnection *cnc,
						     const gchar **args,
						     GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_qecho (SqlConsole *console, GdaConnection *cnc,
						      const gchar **args,
						      GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_list_dsn (SqlConsole *console, GdaConnection *cnc,
							 const gchar **args,
							 GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_create_dsn (SqlConsole *console, GdaConnection *cnc,
							   const gchar **args,
							   GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_remove_dsn (SqlConsole *console, GdaConnection *cnc,
							   const gchar **args,
							   GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_list_providers (SqlConsole *console, GdaConnection *cnc,
							       const gchar **args,
							       GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_manage_cnc (SqlConsole *console, GdaConnection *cnc,
							   const gchar **args,
							   GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_close_cnc (SqlConsole *console, GdaConnection *cnc,
							  const gchar **args,
							  GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_bind_cnc (SqlConsole *console, GdaConnection *cnc,
							 const gchar **args,
							 GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_edit_buffer (SqlConsole *console, GdaConnection *cnc,
							    const gchar **args,
							    GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_reset_buffer (SqlConsole *console, GdaConnection *cnc,
							     const gchar **args,
							     GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_show_buffer (SqlConsole *console, GdaConnection *cnc,
							    const gchar **args,
							    GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_exec_buffer (SqlConsole *console, GdaConnection *cnc,
							    const gchar **args,
							    GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_write_buffer (SqlConsole *console, GdaConnection *cnc,
							     const gchar **args,
							     GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_query_buffer_to_dict (SqlConsole *console, GdaConnection *cnc,
								     const gchar **args,
								     GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_query_buffer_from_dict (SqlConsole *console, GdaConnection *cnc,
								       const gchar **args,
								       GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_query_buffer_list_dict (SqlConsole *console, GdaConnection *cnc,
								       const gchar **args,
								       GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_query_buffer_delete_dict (SqlConsole *console, GdaConnection *cnc,
									 const gchar **args,
									 GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_set (SqlConsole *console, GdaConnection *cnc,
						    const gchar **args,
						    GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_unset (SqlConsole *console, GdaConnection *cnc,
						      const gchar **args,
						      GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_graph (SqlConsole *console, GdaConnection *cnc,
						      const gchar **args,
						      GError **error, gpointer data);
#ifdef HAVE_LIBSOUP
static GdaInternalCommandResult *extra_command_httpd (SqlConsole *console, GdaConnection *cnc,
						      const gchar **args,
						      GError **error, gpointer data);
#endif

#ifdef NONE
static GdaInternalCommandResult *extra_command_lo_update (SqlConsole *console, GdaConnection *cnc,
							  const gchar **args,
							  GError **error, gpointer data);
#endif
static GdaInternalCommandResult *extra_command_export (SqlConsole *console, GdaConnection *cnc,
						       const gchar **args,
						       GError **error, gpointer data);
static GdaInternalCommandResult *extra_command_set2 (SqlConsole *console, GdaConnection *cnc,
						     const gchar **args,
						     GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_declare_fk (SqlConsole *console, GdaConnection *cnc,
							   const gchar **args,
							   GError **error, gpointer data);

static GdaInternalCommandResult *extra_command_undeclare_fk (SqlConsole *console, GdaConnection *cnc,
							     const gchar **args,
							     GError **error, gpointer data);

static GdaInternalCommandsList *
build_internal_commands_list (void)
{
	GdaInternalCommandsList *commands = g_new0 (GdaInternalCommandsList, 1);
	GdaInternalCommand *c;

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [FILE]"), "s");
	c->description = _("Show commands history, or save it to file");
	c->args = NULL;
	c->command_func = gda_internal_command_history;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [META DATA TYPE]"), "meta");
	c->description = _("Force reading the database meta data (or part of the meta data, ex:\"tables\")");
	c->args = NULL;
	c->command_func = gda_internal_command_dict_sync;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s <fkname> <tableA>(<colA>,...) <tableB>(<colB>,...)"), "fkdeclare");
	c->description = _("Declare a new foreign key (not actually in database): tableA references tableB");
	c->args = NULL;
	c->command_func = extra_command_declare_fk;
	c->user_data = NULL;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s <fkname> <tableA> <tableB>"), "fkundeclare");
	c->description = _("Un-declare a foreign key (not actually in database)");
	c->args = NULL;
	c->command_func = extra_command_undeclare_fk;
	c->user_data = NULL;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [TABLE]"), "dt");
	c->description = _("List all tables (or named table)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_tables;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [VIEW]"), "dv");
	c->description = _("List all views (or named view)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_views;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [SCHEMA]"), "dn");
	c->description = _("List all schemas (or named schema)");
	c->args = NULL;
	c->command_func = gda_internal_command_list_schemas;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [OBJ_NAME|SCHEMA.*]"), "d");
	c->description = _("Describe object or full list of objects");
	c->args = NULL;
	c->command_func = gda_internal_command_detail;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [TABLE1 [TABLE2...]]"), "graph");
	c->description = _("Create a graph of all or the listed tables");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_graph;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = FALSE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

#ifdef HAVE_LIBSOUP
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Information");
	c->name = g_strdup_printf (_("%s [port [authentication token]]"), "http");
	c->description = _("Start/stop embedded HTTP server (on given port or on 12345 by default)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_httpd;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);
#endif

	/* specific commands */
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [CNC_NAME [DSN|CONNECTION STRING]]"), "c");
	c->description = _("Opens a new connection or lists opened connections");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_manage_cnc;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [CNC_NAME]"), "close");
	c->description = _("Close a connection");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_close_cnc;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s CNC_NAME CNC_NAME1 CNC_NAME2 [CNC_NAME ...]"), "bind");
	c->description = _("Bind several connections together into the CNC_NAME virtual connection");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc)extra_command_bind_cnc;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [DSN]"), "l");
	c->description = _("List all DSN (or named DSN's attributes)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_dsn;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s DSN_NAME DSN_DEFINITION [DESCRIPTION]"), "lc");
	c->description = _("Create (or modify) a DSN");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_create_dsn;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s DSN_NAME [DSN_NAME...]"), "lr");
	c->description = _("Remove a DSN");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_remove_dsn;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("DSN (data sources) management");
	c->name = g_strdup_printf (_("%s [PROVIDER]"), "lp");
	c->description = _("List all installed database providers (or named one's attributes)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_list_providers;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s FILE"), "i");
	c->description = _("Execute commands from file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_input;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [FILE]"), "o");
	c->description = _("Send output to a file or |pipe");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_output;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [TEXT]"), "echo");
	c->description = _("Send output to stdout");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_echo;
	c->user_data = NULL;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Input/Output");
	c->name = g_strdup_printf (_("%s [TEXT]"), "qecho");
	c->description = _("Send output to output stream");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_qecho;
	c->user_data = NULL;
	c->arguments_delimiter_func = args_as_string_func;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "q";
	c->description = _("Quit");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_quit;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = g_strdup_printf (_("%s [DIR]"), "cd");
	c->description = _("Change the current working directory");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_cd;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "copyright";
	c->description = _("Show usage and distribution terms");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_copyright;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [FILE]"), "e");
	c->description = _("Edit the query buffer (or file) with external editor");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_edit_buffer;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [FILE]"), "qr");
	c->description = _("Reset the query buffer (fill buffer with contents of file)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_reset_buffer;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = "qp";
	c->description = _("Show the contents of the query buffer");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_show_buffer;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [QUERY_BUFFER_NAME]"), "g");
	c->description = _("Execute contents of query buffer, or named query buffer");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_exec_buffer;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s FILE"), "qw");
	c->description = _("Write query buffer to file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_write_buffer;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "qs");
	c->description = _("Save query buffer to dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_to_dict;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "ql");
	c->description = _("Load query buffer from dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_from_dict;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s QUERY_BUFFER_NAME"), "qd");
	c->description = _("Delete query buffer from dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_delete_dict;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s"), "qa");
	c->description = _("List all saved query buffers in dictionary");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_query_buffer_list_dict;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME [VALUE|_null_]]"), "set");
	c->description = _("Set or show internal parameter, or list all if no parameters");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set;
	c->user_data = NULL;
	c->arguments_delimiter_func = args_as_string_set;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME]"), "unset");
	c->description = _("Unset (delete) internal named parameter (or all parameters)");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_unset;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Formatting");
	c->name = "H [HTML|XML|CSV|DEFAULT]";
	c->description = _("Set output format");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set_output_format;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	/*
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s FILE TABLE BLOB_COLUMN ROW_CONDITION"), "lo_update");
	c->description = _("Import a blob into the database");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_lo_update;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);
	*/	

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s [NAME|TABLE COLUMN ROW_CONDITION] FILE"), "export");
	c->description = _("Export internal parameter or table's value to the FILE file");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_export;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("Query buffer");
	c->name = g_strdup_printf (_("%s NAME [FILE|TABLE COLUMN ROW_CONDITION]"), "setex");
	c->description = _("Set internal parameter as the contents of the FILE file or from an existing table's value");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) extra_command_set2;
	c->user_data = NULL;
	c->arguments_delimiter_func = NULL;
	c->unquote_args = TRUE;
	c->limit_to_main = TRUE;
	commands->commands = g_slist_prepend (commands->commands, c);

	/* comes last */
	c = g_new0 (GdaInternalCommand, 1);
	c->group = _("General");
	c->name = "?";
	c->description = _("List all available commands");
	c->args = NULL;
	c->command_func = (GdaInternalCommandFunc) gda_internal_command_help;
	c->user_data = commands;
	c->arguments_delimiter_func = NULL;
	c->limit_to_main = FALSE;
	commands->commands = g_slist_prepend (commands->commands, c);

	return commands;
}

static GdaInternalCommandResult *
extra_command_set_output (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			  GError **error, G_GNUC_UNUSED gpointer data)
{
	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	if (set_output_file (args[0], error)) {
		GdaInternalCommandResult *res;
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static GdaInternalCommandResult *
extra_command_set_output_format (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
				 GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	const gchar *format = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (args[0] && *args[0])
		format = args[0];
	
	main_data->output_format = OUTPUT_FORMAT_DEFAULT;
	if (format) {
		if ((*format == 'X') || (*format == 'x'))
			main_data->output_format = OUTPUT_FORMAT_XML;
		else if ((*format == 'H') || (*format == 'h'))
			main_data->output_format = OUTPUT_FORMAT_HTML;
		else if ((*format == 'D') || (*format == 'd'))
			main_data->output_format = OUTPUT_FORMAT_DEFAULT;
		else if ((*format == 'C') || (*format == 'c'))
			main_data->output_format = OUTPUT_FORMAT_CSV;
		else {
			g_set_error (error, 0, 0,
				     _("Unknown output format: '%s', reset to default"), format);
			return NULL;
		}
	}

	if (!main_data->output_stream) {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new ("");
		switch (main_data->output_format) {
		case OUTPUT_FORMAT_DEFAULT:
			g_string_assign (res->u.txt, ("Output format is default\n"));
			break;
		case OUTPUT_FORMAT_HTML:
			g_string_assign (res->u.txt, ("Output format is HTML\n"));
			break;
		case OUTPUT_FORMAT_XML:
			g_string_assign (res->u.txt, ("Output format is XML\n"));
			break;
		case OUTPUT_FORMAT_CSV:
			g_string_assign (res->u.txt, ("Output format is CSV\n"));
			break;
		default:
			TO_IMPLEMENT;
		}
	}
	else {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	return res;
}

static gboolean
idle_read_input_stream (G_GNUC_UNUSED gpointer data)
{
	if (main_data->input_stream) {
		gchar *cmde;
		cmde = input_from_stream (main_data->input_stream);
		if (cmde) {
			gboolean command_ok;
			treat_line_func (cmde, &command_ok);
			g_free (cmde);
			if (command_ok)
				return TRUE; /* potentially some more work to do from the stream */
			else 
				goto stop;
		}
		else 
			goto stop;
	}

 stop:
	compute_prompt (NULL, prompt, main_data->partial_command == NULL ? FALSE : TRUE);
	g_print ("\n%s", prompt->str);
	fflush (NULL);
	set_input_file (NULL, NULL);
	return FALSE; /* stop calling this function */
}

static GdaInternalCommandResult *
extra_command_set_input (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			 GError **error, G_GNUC_UNUSED gpointer data)
{
	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (set_input_file (args[0], error)) {
		GdaInternalCommandResult *res;
		
		g_idle_add ((GSourceFunc) idle_read_input_stream, NULL);

		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	else
		return NULL;
}

static GdaInternalCommandResult *
extra_command_echo (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
		    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
	res->u.txt = g_string_new (args[0]);
	if (args[0][strlen (args[0]) - 1] != '\n')
		g_string_append_c (res->u.txt, '\n');
	return res;
}

static GdaInternalCommandResult *
extra_command_qecho (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
		     G_GNUC_UNUSED GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (args[0]);
	return res;
}

static GdaInternalCommandResult *
extra_command_list_dsn (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GList *list = NULL;
	GdaDataModel *dsn_list = NULL, *model = NULL;

	if (args[0]) {
		/* details about a DSN */
		GdaDataModel *model;
		model = config_info_detail_dsn (args[0], error);
		if (model) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
			res->u.model = model;
			return res;
		}
		else
			return NULL;
	}
	else {
		gint i, nrows;
		
		dsn_list = gda_config_list_dsn ();
		nrows = gda_data_model_get_n_rows (dsn_list);

		model = gda_data_model_array_new_with_g_types (3,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("DSN"));
		gda_data_model_set_column_title (model, 1, _("Description"));
		gda_data_model_set_column_title (model, 2, _("Provider"));
		g_object_set_data (G_OBJECT (model), "name", _("DSN list"));
		
		for (i =0; i < nrows; i++) {
			const GValue *value;
			list = NULL;
			value = gda_data_model_get_value_at (dsn_list, 0, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (NULL, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 2, i, error);
			if (!value) 
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			value = gda_data_model_get_value_at (dsn_list, 1, i, error);
			if (!value)
				goto onerror;
			list = g_list_append (list, gda_value_copy (value));
			
			if (gda_data_model_append_values (model, list, error) == -1)
				goto onerror;
			
			g_list_foreach (list, (GFunc) gda_value_free, NULL);
			g_list_free (list);
		}
		
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		g_object_unref (dsn_list);
		
		return res;
	}

 onerror:
	if (list) {
		g_list_foreach (list, (GFunc) gda_value_free, NULL);
		g_list_free (list);
	}
	g_object_unref (dsn_list);
	g_object_unref (model);
	return NULL;
}

static GdaInternalCommandResult *
extra_command_create_dsn (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			  const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;
	GdaDsnInfo newdsn;
	gchar *real_cnc, *real_provider, *user, *pass;

	if (!args[0] || !args[1]) {
		g_set_error (error, 0, 0, "%s", 
			     _("Missing arguments"));
		return NULL;
	}

	newdsn.name = (gchar *) args [0];
	gda_connection_string_split ((gchar *) args[1], &real_cnc, &real_provider, &user, &pass);
	newdsn.provider = real_provider;
	newdsn.description = (gchar*) args[2];
	
	newdsn.cnc_string = real_cnc;
	newdsn.auth_string = NULL;
	if (user) {
		gchar *tmp;
		tmp = gda_rfc1738_encode (user);
		newdsn.auth_string = g_strdup_printf ("USERNAME=%s", tmp);
		g_free (tmp);
	}
	newdsn.is_system = FALSE;

	if (!newdsn.provider) {
		g_set_error (error, 0, 0, "%s", 
			     _("Missing provider name"));
	}
	else if (gda_config_define_dsn (&newdsn, error)) {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}

	g_free (real_cnc);
	g_free (real_provider);
	g_free (user);
	g_free (pass);

	return res;
}

static GdaInternalCommandResult *
extra_command_remove_dsn (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			  const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	gint i;

	if (!args[0]) {
		g_set_error (error, 0, 0, "%s", 
			     _("Missing DSN name"));
		return NULL;
	}
	for (i = 0; args [i]; i++) {
		if (! gda_config_remove_dsn (args[i], error))
			return NULL;
	}

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;	
}

/*
 * @console, @cnc and @data are unused here
 */
static GdaInternalCommandResult *
extra_command_list_providers (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			      const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	GdaDataModel *model;

	if (args[0])
		model = config_info_detail_provider (args[0], error);
	else
		model = config_info_list_all_providers ();
		
	if (model) {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
	return NULL;
}

static void
vconnection_hub_foreach_cb (G_GNUC_UNUSED GdaConnection *cnc, const gchar *ns, GString *string)
{
	if (string->len > 0)
		g_string_append_c (string, '\n');
	g_string_append_printf (string, "namespace %s", ns);
}

static 
GdaInternalCommandResult *
extra_command_manage_cnc (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			  GError **error, G_GNUC_UNUSED gpointer data)
{
	/* arguments:
	 * 0 = connection name
	 * 1 = DSN or direct string
	 * 2 = user
	 * 3 = password
	 */
	if (args[0]) {
		if (args [1]) {
			/* open a new connection */
			const gchar *user = NULL, *pass = NULL;
			ConnectionSetting *cs;
			
			cs = find_connection_from_name (args[0]);
			if (cs) {
				g_set_error (error, 0, 0,
					     _("A connection named '%s' already exists"), args[0]);
				return NULL;
			}

			if (args[2]) {
				user = args[2];
				if (args[3])
					pass = args[3];
			}
			cs = open_connection (console, args[0], args[1], error);
			if (cs) {
				GdaInternalCommandResult *res;
				
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
				return res;
			}
			else
				return NULL;
		}
		else {
			/* switch to another already opened connection */
			ConnectionSetting *cs;

			cs = find_connection_from_name (args[0]);
			if (cs) {
				GdaInternalCommandResult *res;
				
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
				if (console)
					console->current = cs;
				else
					main_data->current = cs;
				return res;
			}
			else {
				if (*args [0] == '~') {
					/* find connection for which we want the meta store's connection */
					if (*(args[0] + 1)) {
						cs = find_connection_from_name (args[0] + 1);
						if (!cs) {
							g_set_error (error, 0, 0,
								     _("No connection named '%s' found"), args[0] + 1);
							return NULL;
						}
					}
					else if (console)
						cs = console->current;
					else
						cs = main_data->current;

					if (!cs) {
						g_set_error (error, 0, 0, "%s", 
							     _("No current connection"));
						return NULL;
					}

					/* find if requested connection already exists */
					ConnectionSetting *ncs = NULL;
					if (* (cs->name) == '~') 
						ncs = find_connection_from_name (main_data->current->name + 1);
					else {
						gchar *tmp;
						tmp = g_strdup_printf ("~%s", cs->name);
						ncs = find_connection_from_name (tmp);
						g_free (tmp);
					}
					if (ncs) {
						GdaInternalCommandResult *res;
						
						res = g_new0 (GdaInternalCommandResult, 1);
						res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
						if (console)
							console->current = ncs;
						else
							main_data->current = ncs;
						return res;
					}

					/* open a new connection */
					ncs = g_new0 (ConnectionSetting, 1);
					GdaMetaStore *store;
					GdaInternalCommandResult *res;
					
					ncs->name = g_strdup_printf ("~%s", cs->name);
					store = gda_connection_get_meta_store (cs->cnc);
					ncs->cnc = gda_meta_store_get_internal_connection (store);
					g_object_ref (ncs->cnc);
					ncs->parser = gda_connection_create_parser (ncs->cnc);
					if (!cs->parser)
						cs->parser = gda_sql_parser_new ();
					ncs->query_buffer = NULL;
					ncs->threader = NULL;
					ncs->meta_job_id = 0;
					
					main_data->settings = g_slist_append (main_data->settings, ncs);
					if (console)
						console->current = ncs;
					else
						main_data->current = ncs;

					GError *lerror = NULL;
					if (!main_data->output_stream) {
						g_print (_("Getting database schema information, "
							   "this may take some time... "));
						fflush (stdout);
					}
					if (!gda_connection_update_meta_store (ncs->cnc, NULL, &lerror)) {
						if (!main_data->output_stream) 
							g_print (_("error: %s\n"), 
								 lerror && lerror->message ? lerror->message : _("No detail"));
						if (lerror)
							g_error_free (lerror);
					}
					else
						if (!main_data->output_stream) 
							g_print (_("Done.\n"));
					
					res = g_new0 (GdaInternalCommandResult, 1);
					res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
					return res;
				}

				g_set_error (error, 0, 0,
					     _("No connection named '%s' found"), args[0]);
				return NULL;
			}
		}
	}
	else {
		/* list opened connections */
		GdaDataModel *model;
		GSList *list;
		GdaInternalCommandResult *res;

		if (! main_data->settings) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
			res->u.txt = g_string_new (_("No opened connection"));
			return res;
		}

		model = gda_data_model_array_new_with_g_types (4,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Provider"));
		gda_data_model_set_column_title (model, 2, _("DSN or connection string"));
		gda_data_model_set_column_title (model, 3, _("Username"));
		g_object_set_data (G_OBJECT (model), "name", _("List of opened connections"));

		for (list = main_data->settings; list; list = list->next) {
			ConnectionSetting *cs = (ConnectionSetting *) list->data;
			GValue *value;
			gint row;
			const gchar *cstr;
			GdaServerProvider *prov;
			
			row = gda_data_model_append_row (model, NULL);
			
			value = gda_value_new_from_string (cs->name, G_TYPE_STRING);
			gda_data_model_set_value_at (model, 0, row, value, NULL);
			gda_value_free (value);
			
			prov = gda_connection_get_provider (cs->cnc);
			if (GDA_IS_VPROVIDER_HUB (prov))
				value = gda_value_new_from_string ("", G_TYPE_STRING);
			else
				value = gda_value_new_from_string (gda_connection_get_provider_name (cs->cnc), G_TYPE_STRING);
			gda_data_model_set_value_at (model, 1, row, value, NULL);
			gda_value_free (value);

			if (GDA_IS_VPROVIDER_HUB (prov)) {
				GString *string = g_string_new ("");
				gda_vconnection_hub_foreach (GDA_VCONNECTION_HUB (cs->cnc), 
							     (GdaVConnectionHubFunc) vconnection_hub_foreach_cb, string);
				value = gda_value_new_from_string (string->str, G_TYPE_STRING);
				g_string_free (string, TRUE);
			}
			else {
				cstr = gda_connection_get_dsn (cs->cnc);
				if (cstr)
					value = gda_value_new_from_string (cstr, G_TYPE_STRING);
				else
					value = gda_value_new_from_string (gda_connection_get_cnc_string (cs->cnc), G_TYPE_STRING);
			}
			gda_data_model_set_value_at (model, 2, row, value, NULL);
			gda_value_free (value);

			/* only get USERNAME from the the authentication string */
			GdaQuarkList* ql;
			cstr = gda_connection_get_authentication (cs->cnc);
			ql = gda_quark_list_new_from_string (cstr);
			cstr = gda_quark_list_find (ql, "USERNAME");
			value = gda_value_new_from_string (cstr ? cstr : "", G_TYPE_STRING);
			gda_quark_list_free (ql);
			gda_data_model_set_value_at (model, 3, row, value, NULL);
			gda_value_free (value);
		}
 
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
		return res;
	}
}

static void
conn_closed_cb (GdaConnection *cnc, G_GNUC_UNUSED ConnectionSetting *cs)
{
	extra_command_close_cnc (NULL, cnc, NULL, NULL, NULL);
}

static 
GdaInternalCommandResult *
extra_command_close_cnc (SqlConsole *console, GdaConnection *cnc, const gchar **args, GError **error,
			 G_GNUC_UNUSED gpointer data)
{
	ConnectionSetting *cs = NULL;
	if (args && args[0] && *args[0]) {
		cs = find_connection_from_name (args[0]);
		if (!cs) {
			g_set_error (error, 0, 0,
				     _("No connection named '%s' found"), args[0]);
			return NULL;
		}
	}
	else {
		/* close @cnc */
		GSList *list;
		for (list = main_data->settings; list; list = list->next) {
			if (((ConnectionSetting *) list->data)->cnc == cnc) {
				cs = (ConnectionSetting *) list->data;
				break;
			}
		}

		if (! cs) {
			g_set_error (error, 0, 0, "%s", 
				     _("No connection currently opened"));
			return NULL;
		}
	}
	
	if ((console && (console->current == cs)) ||
	    (!console && (main_data->current == cs))) {
		gint index;
		ConnectionSetting *ncs = NULL;
		index = g_slist_index (main_data->settings, cs);
		if (index == 0)
			ncs = g_slist_nth_data (main_data->settings, index + 1);
		else
			ncs = g_slist_nth_data (main_data->settings, index - 1);
		if (console)
			console->current = ncs;
		else
			main_data->current = ncs;
	}
	main_data->settings = g_slist_remove (main_data->settings, cs);
	connection_settings_free (cs);

	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;
}

static GdaInternalCommandResult *
extra_command_bind_cnc (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			GError **error, G_GNUC_UNUSED gpointer data)
{
	ConnectionSetting *cs = NULL;
	gint i, nargs = g_strv_length ((gchar **) args);
	static GdaVirtualProvider *vprovider = NULL;
	GdaConnection *virtual;
	GString *string;

	if (nargs < 3) {
		g_set_error (error, 0, 0, "%s", 
				     _("Missing required connection names"));
		return NULL;
	}

	/* check for connections existance */
	cs = find_connection_from_name (args[0]);
	if (cs) {
		g_set_error (error, 0, 0,
			     _("A connection named '%s' already exists"), args[0]);
		return NULL;
	}
	if (! connection_name_is_valid (args[0])) {
		g_set_error (error, 0, 0,
			     _("Connection name '%s' is invalid"), args[0]);
		return NULL;
	}
	for (i = 1; i < nargs; i++) {
		cs = find_connection_from_name (args[i]);
		if (!cs) {
			g_set_error (error, 0, 0,
				     _("No connection named '%s' found"), args[i]);
			return NULL;
		}
	}

	/* actual virtual connection creation */
	if (!vprovider) 
		vprovider = gda_vprovider_hub_new ();
	g_assert (vprovider);

	virtual = gda_virtual_connection_open_extended (vprovider, GDA_CONNECTION_OPTIONS_THREAD_SAFE |
							GDA_CONNECTION_OPTIONS_AUTO_META_DATA, NULL);
	if (!virtual) {
		g_set_error (error, 0, 0, "%s", _("Could not create virtual connection"));
		return NULL;
	}
	g_object_set (G_OBJECT (virtual), "execution-timer", TRUE, NULL);
	gda_connection_get_meta_store (virtual); /* force create of meta store */

	/* add existing connections to virtual connection */
	string = g_string_new (_("Bound connections are as:"));
	for (i = 1; i < nargs; i++) {
		cs = find_connection_from_name (args[i]);
		if (!gda_vconnection_hub_add (GDA_VCONNECTION_HUB (virtual), cs->cnc, args[i], error)) {
			g_object_unref (virtual);
			g_string_free (string, TRUE);
			return NULL;
		}
		g_string_append_printf (string, "\n   %s in the '%s' namespace", args[i], args[i]);
        }

	cs = g_new0 (ConnectionSetting, 1);
	cs->name = g_strdup (args[0]);
	cs->cnc = virtual;
	cs->parser = gda_connection_create_parser (virtual);
	cs->query_buffer = NULL;
	cs->threader = NULL;
	cs->meta_job_id = 0;

	main_data->settings = g_slist_append (main_data->settings, cs);
	if (console)
		console->current = cs;
	else
		main_data->current = cs;

	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = string;
	return res;
}

static GdaInternalCommandResult *
extra_command_copyright (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			 G_GNUC_UNUSED const gchar **args, G_GNUC_UNUSED GError **error,
			 G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new ("This program is free software; you can redistribute it and/or modify\n"
				   "it under the terms of the GNU General Public License as published by\n"
				   "the Free Software Foundation; either version 2 of the License, or\n"
				   "(at your option) any later version.\n\n"
				   "This program is distributed in the hope that it will be useful,\n"
				   "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
				   "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
				   "GNU General Public License for more details.\n");
	return res;
}

static GdaInternalCommandResult *
extra_command_quit (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, G_GNUC_UNUSED const gchar **args,
		    G_GNUC_UNUSED GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EXIT;
	return res;
}

static GdaInternalCommandResult *
extra_command_cd (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
		  GError **error, G_GNUC_UNUSED gpointer data)
{
	const gchar *dir = NULL;
#define DIR_LENGTH 256
	static char start_dir[DIR_LENGTH];
	static gboolean init_done = FALSE;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!init_done) {
		init_done = TRUE;
		memset (start_dir, 0, DIR_LENGTH);
		if (getcwd (start_dir, DIR_LENGTH) != NULL) {
			/* default to $HOME */
#ifdef G_OS_WIN32
			TO_IMPLEMENT;
			strncpy (start_dir, "/", 2);
#else
			struct passwd *pw;
			
			pw = getpwuid (geteuid ());
			if (!pw) {
				g_set_error (error, 0, 0, _("Could not get home directory: %s"), strerror (errno));
				return NULL;
			}
			else {
				gsize l = strlen (pw->pw_dir);
				if (l > DIR_LENGTH - 1)
					strncpy (start_dir, "/", 2);
				else
					strncpy (start_dir, pw->pw_dir, l + 1);
			}
#endif
		}
	}

	if (args[0]) 
		dir = args[0];
	else 
		dir = start_dir;

	if (dir) {
		if (chdir (dir) == 0) {
			GdaInternalCommandResult *res;

			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
			res->u.txt = g_string_new ("");
			g_string_append_printf (res->u.txt, _("Working directory is now: %s"), dir);
			return res;
		}
		else
			g_set_error (error, 0, 0, _("Could not change working directory to '%s': %s"),
				     dir, strerror (errno));
	}

	return NULL;
}

static GdaInternalCommandResult *
extra_command_edit_buffer (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			   GError **error, G_GNUC_UNUSED gpointer data)
{
	gchar *filename = NULL;
	static gchar *editor_name = NULL;
	gchar *command = NULL;
	gint systemres;
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		goto end_of_command;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0])
		filename = (gchar *) args[0];
	else {
		/* use a temp file */
		gint fd;
		fd = g_file_open_tmp (NULL, &filename, error);
		if (fd < 0)
			goto end_of_command;
		if (write (fd, main_data->current->query_buffer->str, 
			   main_data->current->query_buffer->len) != (gint)main_data->current->query_buffer->len) {
			g_set_error (error, 0, 0,
				     _("Could not write to temporary file '%s': %s"),
				     filename, strerror (errno));
			close (fd);
			goto end_of_command;
		}
		close (fd);
	}

	if (!editor_name) {
		editor_name = getenv("GDA_SQL_EDITOR");
		if (!editor_name)
			editor_name = getenv("EDITOR");
		if (!editor_name)
			editor_name = getenv("VISUAL");
		if (!editor_name) {
#ifdef G_OS_WIN32
			editor_name = "notepad.exe";
#else
			editor_name = "vi";
#endif
		}
	}
#ifdef G_OS_WIN32
#ifndef __CYGWIN__
#define SYSTEMQUOTE "\""
#else
#define SYSTEMQUOTE ""
#endif
	command = g_strdup_printf ("%s\"%s\" \"%s\"%s", SYSTEMQUOTE, editor_name, filename, SYSTEMQUOTE);
#else
	command = g_strdup_printf ("exec %s '%s'", editor_name, filename);
#endif

	systemres = system (command);
	if (systemres == -1) {
                g_set_error (error, 0, 0,
			     _("could not start editor '%s'"), editor_name);
		goto end_of_command;
	}
        else if (systemres == 127) {
		g_set_error (error, 0, 0,
			     _("Could not start /bin/sh"));
		goto end_of_command;
	}
	else {
		if (! args[0]) {
			gchar *str;
			
			if (!g_file_get_contents (filename, &str, NULL, error))
				goto end_of_command;
			g_string_assign (main_data->current->query_buffer, str);
			g_free (str);
		}
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

 end_of_command:

	g_free (command);
	if (! args[0]) {
		g_unlink (filename);
		g_free (filename);
	}

	return res;
}

static GdaInternalCommandResult *
extra_command_reset_buffer (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			    GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");
	else 
		g_string_assign (main_data->current->query_buffer, "");

	if (args[0]) {
		const gchar *filename = NULL;
		gchar *str;

		filename = args[0];
		if (!g_file_get_contents (filename, &str, NULL, error))
			return NULL;

		g_string_assign (main_data->current->query_buffer, str);
		g_free (str);
	}
	
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;

	return res;
}

static GdaInternalCommandResult *
extra_command_show_buffer (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
			   G_GNUC_UNUSED const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_TXT;
	res->u.txt = g_string_new (main_data->current->query_buffer->str);

	return res;
}

static GdaInternalCommandResult *
extra_command_exec_buffer (SqlConsole *console, GdaConnection *cnc, const gchar **args,
			   GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		/* load named query buffer first */
		res = extra_command_query_buffer_from_dict (console, cnc, args, error, data);
		if (!res)
			return NULL;
		gda_internal_command_exec_result_free (res);
		res = NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");
	if (*main_data->current->query_buffer->str != 0)
		res = command_execute (NULL, main_data->current->query_buffer->str, error);
	else {
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}

	return res;
}

static GdaInternalCommandResult *
extra_command_write_buffer (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
			    GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");
	if (!args[0]) 
		g_set_error (error, 0, 0, "%s",  
			     _("Missing FILE to write to"));
	else {
		if (g_file_set_contents (args[0], main_data->current->query_buffer->str, -1, error)) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}

	return res;
}

#define QUERY_BUFFERS_TABLE_NAME "gda_sql_query_buffers"
#define QUERY_BUFFERS_TABLE_DESC \
	"<table name=\"" QUERY_BUFFERS_TABLE_NAME "\"> "			    \
	"   <column name=\"id\" type=\"gint\" pkey=\"TRUE\" autoinc=\"TRUE\"/>"	    \
	"   <column name=\"name\"/>"				    \
	"   <column name=\"sql\"/>"				    \
	"</table>"
#define QUERY_BUFFERS_TABLE_INSERT \
	"INSERT INTO " QUERY_BUFFERS_TABLE_NAME " (name, sql) VALUES (##name::string, ##sql::string)"
#define QUERY_BUFFERS_TABLE_SELECT \
	"SELECT name, sql FROM " QUERY_BUFFERS_TABLE_NAME " ORDER BY name"
#define QUERY_BUFFERS_TABLE_SELECT_ONE \
	"SELECT sql FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"
#define QUERY_BUFFERS_TABLE_DELETE \
	"DELETE FROM " QUERY_BUFFERS_TABLE_NAME " WHERE name = ##name::string"

static GdaInternalCommandResult *
extra_command_query_buffer_list_dict (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, G_GNUC_UNUSED const gchar **args,
				      GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	/* Meta store's init */
	GdaMetaStore *mstore;
	mstore = gda_connection_get_meta_store (main_data->current->cnc);
	if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
		g_set_error (error, 0, 0, "%s", 
			     _("Can't initialize dictionary to store query buffers"));
		return NULL;
	}
		
	/* actual list retrieval */
	static GdaStatement *sel_stmt = NULL;
	GdaDataModel *model;
	if (!sel_stmt) {
		sel_stmt = gda_sql_parser_parse_string (main_data->current->parser, 
							QUERY_BUFFERS_TABLE_SELECT, NULL, NULL);
		g_assert (sel_stmt);
	}

	GdaConnection *store_cnc;
	store_cnc = gda_meta_store_get_internal_connection (mstore);
	model = gda_connection_statement_execute_select (store_cnc, sel_stmt, NULL, error);
	if (!model)
		return NULL;

	gda_data_model_set_column_title (model, 0, _("Query buffer name"));
	gda_data_model_set_column_title (model, 1, _("SQL"));
	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
	res->u.model = model;

	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_to_dict (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
				    GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");
	if (*main_data->current->query_buffer->str != 0) {
		/* find a suitable name */
		gchar *qname;
		if (args[0] && *args[0]) 
			qname = g_strdup ((gchar *) args[0]);
		else {
			g_set_error (error, 0, 0, "%s", 
				     _("Missing query buffer name"));
			return NULL;
		}

		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (main_data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0, "%s", 
				     _("Can't initialize dictionary to store query buffers"));
			g_free (qname);
			return NULL;
		}
		
		/* actual store of the statement */
		static GdaStatement *ins_stmt = NULL, *del_stmt;
		static GdaSet *ins_params = NULL;
		if (!ins_stmt) {
			ins_stmt = gda_sql_parser_parse_string (main_data->current->parser, 
								QUERY_BUFFERS_TABLE_INSERT, NULL, NULL);
			g_assert (ins_stmt);
			g_assert (gda_statement_get_parameters (ins_stmt, &ins_params, NULL));

			del_stmt = gda_sql_parser_parse_string (main_data->current->parser, 
								QUERY_BUFFERS_TABLE_DELETE, NULL, NULL);
			g_assert (del_stmt);
		}

		if (! gda_set_set_holder_value (ins_params, error, "name", qname) ||
		    ! gda_set_set_holder_value (ins_params, error, "sql", main_data->current->query_buffer->str)) {
			g_free (qname);
			return NULL;
		}
		g_free (qname);
		
		GdaConnection *store_cnc;
		gboolean intrans;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		intrans = gda_connection_begin_transaction (store_cnc, NULL,
							    GDA_TRANSACTION_ISOLATION_UNKNOWN, NULL);

		if ((gda_connection_statement_execute_non_select (store_cnc, del_stmt, ins_params,
								  NULL, error) == -1) ||
		    (gda_connection_statement_execute_non_select (store_cnc, ins_stmt, ins_params,
								  NULL, error) == -1)) {
			if (intrans)
				gda_connection_rollback_transaction (store_cnc, NULL, NULL);
			return NULL;
		}
		if (intrans)
			gda_connection_commit_transaction (store_cnc, NULL, NULL);

		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, 0, 0, "%s", 
			     _("Query buffer is empty"));

	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_from_dict (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
				      const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0]) {
		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (main_data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0, "%s", 
				     _("Can't initialize dictionary to store query buffers"));
			return NULL;
		}
		
		/* query retrieval */
		static GdaStatement *sel_stmt = NULL;
		static GdaSet *sel_params = NULL;
		GdaDataModel *model;
		const GValue *cvalue;
		if (!sel_stmt) {
			sel_stmt = gda_sql_parser_parse_string (main_data->current->parser, 
								QUERY_BUFFERS_TABLE_SELECT_ONE, NULL, NULL);
			g_assert (sel_stmt);
			g_assert (gda_statement_get_parameters (sel_stmt, &sel_params, NULL));
		}

		if (! gda_set_set_holder_value (sel_params, error, "name", args[0]))
			return NULL;

		GdaConnection *store_cnc;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		model = gda_connection_statement_execute_select (store_cnc, sel_stmt, sel_params, error);
		if (!model)
			return NULL;
		
		if ((gda_data_model_get_n_rows (model) == 1) &&
		    (cvalue = gda_data_model_get_value_at (model, 0, 0, NULL))) {
			g_string_assign (main_data->current->query_buffer, g_value_get_string (cvalue));
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, 0, 0,
				     _("Could not find query buffer named '%s'"), args[0]);
		g_object_unref (model);
	}
	else
		g_set_error (error, 0, 0, "%s", 
			     _("Missing query buffer name"));
		
	return res;
}

static GdaInternalCommandResult *
extra_command_query_buffer_delete_dict (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc,
					const gchar **args, GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (!main_data->current->query_buffer) 
		main_data->current->query_buffer = g_string_new ("");

	if (args[0] && *args[0]) {
		/* Meta store's init */
		GdaMetaStore *mstore;
		mstore = gda_connection_get_meta_store (main_data->current->cnc);
		if (!gda_meta_store_schema_add_custom_object (mstore, QUERY_BUFFERS_TABLE_DESC, NULL)) {
			g_set_error (error, 0, 0, "%s", 
				     _("Can't initialize dictionary to store query buffers"));
			return NULL;
		}
		
		/* query retrieval */
		static GdaStatement *del_stmt = NULL;
		static GdaSet *del_params = NULL;
		if (!del_stmt) {
			del_stmt = gda_sql_parser_parse_string (main_data->current->parser, 
								QUERY_BUFFERS_TABLE_DELETE, NULL, NULL);
			g_assert (del_stmt);
			g_assert (gda_statement_get_parameters (del_stmt, &del_params, NULL));
		}

		if (! gda_set_set_holder_value (del_params, error, "name", args[0]))
			return NULL;

		GdaConnection *store_cnc;
		store_cnc = gda_meta_store_get_internal_connection (mstore);
		if (gda_connection_statement_execute_non_select (store_cnc, del_stmt, del_params,
								 NULL, error) == -1)
			return NULL;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else
		g_set_error (error, 0, 0, "%s", 
			     _("Missing query buffer name"));
		
	return res;
}

static void foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model);
static GdaInternalCommandResult *
extra_command_set (G_GNUC_UNUSED SqlConsole *console, GdaConnection *cnc, const gchar **args,
		   GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *value = NULL;

	if (args[0] && *args[0]) {
		pname = args[0];
		if (args[1] && *args[1])
			value = args[1];
	}

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		if (param) {
			if (value) {
				/* set param's value */
				if (!strcmp (value, "_null_")) {
					if (! gda_holder_set_value (param, NULL, error))
						return NULL;
				}
				else {
					GdaServerProvider *prov;
					GdaDataHandler *dh;
					GValue *gvalue;

					prov = gda_connection_get_provider (cnc);
					dh = gda_server_provider_get_data_handler_g_type (prov, cnc,
											 gda_holder_get_g_type (param));
					gvalue = gda_data_handler_get_value_from_str (dh, value, gda_holder_get_g_type (param));
					if (! gda_holder_take_value (param, gvalue, error))
						return NULL;
				}

				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
			}
			else {
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_SET;
				res->u.set = gda_set_new (NULL);
				gda_set_add_holder (res->u.set, gda_holder_copy (param));
			}
		}
		else {
			if (value) {
				/* create parameter */
				if (!strcmp (value, "_null_"))
					value = NULL;
				param = gda_holder_new_string (pname, value);
				g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
			}
			else 
				g_set_error (error, 0, 0,
					     _("No parameter named '%s' defined"), pname);
		}
	}
	else {
		/* list parameter's values */
		GdaDataModel *model;
		model = gda_data_model_array_new_with_g_types (2,
							       G_TYPE_STRING,
							       G_TYPE_STRING);
		gda_data_model_set_column_title (model, 0, _("Name"));
		gda_data_model_set_column_title (model, 1, _("Value"));
		g_object_set_data (G_OBJECT (model), "name", _("List of defined parameters"));
		g_hash_table_foreach (main_data->parameters, (GHFunc) foreach_param_set, model);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL;
		res->u.model = model;
	}
		
	return res;
}

static void 
foreach_param_set (const gchar *pname, GdaHolder *param, GdaDataModel *model)
{
	gint row;
	gchar *str;
	GValue *value;
	row = gda_data_model_append_row (model, NULL);

	value = gda_value_new_from_string (pname, G_TYPE_STRING);
	gda_data_model_set_value_at (model, 0, row, value, NULL);
	gda_value_free (value);
			
	str = gda_holder_get_value_str (param, NULL);
	value = gda_value_new_from_string (str ? str : "(NULL)", G_TYPE_STRING);
	gda_data_model_set_value_at (model, 1, row, value, NULL);
	gda_value_free (value);
}

typedef struct {
	gchar *fkname;
	gchar *table;
	gchar *ref_table;
	GArray *columns;
	GArray *ref_columns;
} FkDeclData;

static void
fk_decl_data_free (FkDeclData *data)
{
	g_free (data->fkname);
	g_free (data->table);
	g_free (data->ref_table);
	if (data->columns) {
		gint i;
		for (i = 0; i < data->columns->len; i++)
			g_free (g_array_index (data->columns, gchar*, i));
		g_array_free (data->columns, TRUE);
	}
	if (data->ref_columns) {
		gint i;
		for (i = 0; i < data->ref_columns->len; i++)
			g_free (g_array_index (data->ref_columns, gchar*, i));
		g_array_free (data->ref_columns, TRUE);
	}
	g_free (data);
}

static FkDeclData *
parse_fk_decl_spec (const gchar *spec, gboolean columns_required, GError **error)
{
	FkDeclData *decldata;
	decldata = g_new0 (FkDeclData, 1);

	gchar *ptr, *dspec, *start, *subptr, *substart;

	if (!spec || !*spec) {
		g_set_error (error, 0, 0,
			     _("Missing foreign key declaration specification"));
		return NULL;
	}
	dspec = g_strstrip (g_strdup (spec));

	/* FK name */
	for (ptr = dspec; *ptr && !g_ascii_isspace (*ptr); ptr++) {
		if (! g_ascii_isalnum (*ptr) && (*ptr != '_'))
			goto onerror;
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	decldata->fkname = g_strstrip (g_strdup (dspec));
#ifdef GDA_DEBUG_NO
	g_print ("KFNAME [%s]\n", decldata->fkname);
#endif

	/* table name */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	if (!*ptr)
		goto onerror;
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("TABLE [%s]\n", decldata->table);
#endif

	if (columns_required) {
		/* columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("COLS [%s]\n", start);
#endif
		substart = start;
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->columns)
				decldata->columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->columns)
			goto onerror;
	}

	/* ref table */
	start = ++ptr;
	if (columns_required)
		for (ptr = start; *ptr && (*ptr != '('); ptr++);
	else {
		for (ptr = start; *ptr && g_ascii_isspace (*ptr); ptr++);
		start = ptr;
		for (; *ptr && !g_ascii_isspace (*ptr); ptr++);
	}
	*ptr = 0;
	if (!*start)
		goto onerror;
	decldata->ref_table = g_strstrip (g_strdup (start));
#ifdef GDA_DEBUG_NO
	g_print ("REFTABLE [%s]\n", decldata->ref_table);
#endif

	if (columns_required) {
		/* ref columns names */
		start = ++ptr;
		for (ptr = start; *ptr && (*ptr != ')'); ptr++);
		if (!*ptr)
			goto onerror;
		*ptr = 0;
#ifdef GDA_DEBUG_NO
		g_print ("REF COLS [%s]\n", start);
#endif
		for (substart = start; substart < ptr; substart = ++subptr) {
			gchar *tmp;
			for (subptr = substart; *subptr && (*subptr != ','); subptr++);
			*subptr = 0;
			if (!decldata->ref_columns)
				decldata->ref_columns = g_array_new (FALSE, FALSE, sizeof (gchar*));
			tmp = g_strstrip (g_strdup (substart));
			g_array_append_val (decldata->ref_columns, tmp);
#ifdef GDA_DEBUG_NO
			g_print ("  COL [%s]\n", tmp);
#endif
		}
		if (! decldata->ref_columns)
			goto onerror;
	}

	g_free (dspec);

	if (columns_required && (decldata->columns->len != decldata->ref_columns->len))
		goto onerror;

	return decldata;

 onerror:
	fk_decl_data_free (decldata);
	g_set_error (error, 0, 0,
		     _("Malformed foreign key declaration specification"));
	return NULL;
}

/*
 * decomposes @table and sets the the out* variables
 */
static gboolean
fk_decl_analyse_table_name (const gchar *table, GdaMetaStore *mstore,
			    gchar **out_catalog, gchar **out_schema, gchar **out_table,
			    GError **error)
{
	gchar **id_array;
	gint size = 0, l;

	id_array = gda_sql_identifier_split (table);
	if (id_array) {
		size = g_strv_length (id_array) - 1;
		g_assert (size >= 0);
		l = size;
		*out_table = g_strdup (id_array[l]);
		l--;
		if (l >= 0) {
			*out_schema = g_strdup (id_array[l]);
			l--;
			if (l >= 0)
				*out_catalog = g_strdup (id_array[l]);
		}
		g_strfreev (id_array);
	}
	else {
		g_set_error (error, 0, 0,
			     _("Malformed table name specification '%s'"), table);
		return FALSE;
	}
	return TRUE;
}

static GdaInternalCommandResult *
extra_command_declare_fk (SqlConsole *console, GdaConnection *cnc,
			  const gchar **args,
			  GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (main_data->current->cnc);
		if (! (decldata = parse_fk_decl_spec (args[0], TRUE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gchar **colnames, **ref_colnames;
		gint l;
		gboolean allok;
		colnames = g_new0 (gchar *, decldata->columns->len);
		ref_colnames = g_new0 (gchar *, decldata->columns->len);
		for (l = 0; l < decldata->columns->len; l++) {
			colnames [l] = g_array_index (decldata->columns, gchar*, l);
			ref_colnames [l] = g_array_index (decldata->ref_columns, gchar*, l);
		}

		allok = gda_meta_store_declare_foreign_key (mstore, NULL,
							    decldata->fkname,
							    catalog, schema, table,
							    ref_catalog, ref_schema, ref_table,
							    decldata->columns->len,
							    colnames, ref_colnames, error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		g_free (colnames);
		g_free (ref_colnames);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, 0, 0,
			     _("Missing foreign key name argument"));
	return res;
}

static GdaInternalCommandResult *
extra_command_undeclare_fk (SqlConsole *console, GdaConnection *cnc,
			    const gchar **args,
			    GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (!main_data->current) {
		g_set_error (error, 0, 0, "%s", _("No connection opened"));
		return NULL;
	}

	if (args[0] && *args[0]) {
		FkDeclData *decldata;
		GdaMetaStore *mstore;
		gchar *catalog = NULL, *schema = NULL, *table = NULL;
		gchar *ref_catalog = NULL, *ref_schema = NULL, *ref_table = NULL;

		mstore = gda_connection_get_meta_store (main_data->current->cnc);
		if (! (decldata = parse_fk_decl_spec (args[0], FALSE, error)))
			return NULL;
		
		/* table name */
		if (!fk_decl_analyse_table_name (decldata->table, mstore,
						 &catalog, &schema, &table, error)) {
			fk_decl_data_free (decldata);
			return NULL;
		}
		
		/* ref table name */
		if (!fk_decl_analyse_table_name (decldata->ref_table, mstore,
						 &ref_catalog, &ref_schema, &ref_table, error)) {
			fk_decl_data_free (decldata);
			g_free (catalog);
			g_free (schema);
			g_free (table);
			return NULL;
		}

#ifdef GDA_DEBUG_NO
		g_print ("NOW: %s.%s.%s REFERENCES %s.%s.%s\n",
			 gda_value_stringify (gda_set_get_holder_value (params, "tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "tname")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tcal")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tschema")),
			 gda_value_stringify (gda_set_get_holder_value (params, "ref_tname")));
#endif

		gboolean allok;
		allok = gda_meta_store_undeclare_foreign_key (mstore, NULL,
							      decldata->fkname,
							      catalog, schema, table,
							      ref_catalog, ref_schema, ref_table,
							      error);
		g_free (catalog);
		g_free (schema);
		g_free (table);
		g_free (ref_catalog);
		g_free (ref_schema);
		g_free (ref_table);
		fk_decl_data_free (decldata);

		if (allok) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}
	else
		g_set_error (error, 0, 0,
			     _("Missing foreign key name argument"));
	return res;
}

static const GValue *
get_table_value_at_cell (GdaConnection *cnc, GError **error, G_GNUC_UNUSED MainData *data,
			 const gchar *table, const gchar *column, const gchar *row_cond,
			 GdaDataModel **out_model_of_value)
{
	const GValue *retval = NULL;

	*out_model_of_value = NULL;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rcolumn;
	
	rtable = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);
	rcolumn = gda_sql_identifier_quote (column, cnc, NULL, FALSE, FALSE);
	sql = g_strdup_printf ("SELECT %s FROM %s WHERE %s", rcolumn, rtable, row_cond);
	g_free (rtable);
	g_free (rcolumn);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (main_data->current->parser, sql, &remain, error);
	if (!stmt) {
		g_free (sql);
		return NULL;
	}
	if (remain) {
		g_set_error (error, 0, 0, "%s", 
			     _("Wrong row condition"));
		g_free (sql);
		return NULL;
	}
	g_object_unref (stmt);

	/* execute statement */
	GdaInternalCommandResult *tmpres;
	tmpres = execute_external_command (NULL, sql, error);
	g_free (sql);
	if (!tmpres)
		return NULL;
	gboolean errorset = FALSE;
	if (tmpres->type == GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL) {
		GdaDataModel *model;
		model = tmpres->u.model;
		if (gda_data_model_get_n_rows (model) == 1) {
			retval = gda_data_model_get_value_at (model, 0, 0, error);
			if (!retval)
				errorset = TRUE;
			else
				*out_model_of_value = g_object_ref (model);
		}
	}
	gda_internal_command_exec_result_free (tmpres);

	if (!retval && !errorset)
		g_set_error (error, 0, 0, "%s", 
			     _("No unique row identified"));

	return retval;
}

static GdaInternalCommandResult *
extra_command_set2 (SqlConsole *console, GdaConnection *cnc, const gchar **args,
		    GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;
	const gchar *filename = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *row_cond = NULL;
	gint whichargs = 0;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!cnc) {
                g_set_error (error, 0, 0, "%s", _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                pname = args[0];
                if (args[1] && *args[1]) {
			if (args[2] && *args[2]) {
				table = args[1];
				column = args[2];
				if (args[3] && *args[3]) {
					row_cond = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0, "%s", 
							     _("Too many arguments"));
						return NULL;
					}
					whichargs = 1;
				}
			}
			else {
				filename = args[1];
				whichargs = 2;
			}
		}
        }

	if (whichargs == 1) {
		/* param from an existing blob */
		const GValue *value;
		GdaDataModel *model = NULL;
		value = get_table_value_at_cell (cnc, error, data, table, column, row_cond, &model);
		if (value) {
			GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
			if (param) 
				g_hash_table_remove (main_data->parameters, pname);
		
			param = gda_holder_new (G_VALUE_TYPE (value));
			g_assert (gda_holder_set_value (param, value, NULL));
			g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		if (model)
			g_object_unref (model);
	}
	else if (whichargs == 2) {
		/* param from filename */
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		GValue *bvalue;
		if (param) 
			g_hash_table_remove (main_data->parameters, pname);
		
		param = gda_holder_new (GDA_TYPE_BLOB);
		bvalue = gda_value_new_blob_from_file (filename);
		g_assert (gda_holder_take_value (param, bvalue, NULL));
		g_hash_table_insert (main_data->parameters, g_strdup (pname), param);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
	else 
		g_set_error (error, 0, 0, "%s", 
                             _("Wrong number of arguments"));

	return res;
}

static GdaInternalCommandResult *
extra_command_export (SqlConsole *console, GdaConnection *cnc, const gchar **args,
		      GError **error, gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	const gchar *pname = NULL;
	const gchar *table = NULL;
        const gchar *column = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;
	gint whichargs = 0;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!cnc) {
                g_set_error (error, 0, 0, "%s", _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                table = args[0];
                pname = args[0];
                if (args[1] && *args[1]) {
                        column = args[1];
			filename = args[1];
			if (args[2] && *args[2]) {
				row_cond = args[2];
				if (args[3] && *args[3]) {
					filename = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0, "%s", 
							     _("Too many arguments"));
						return NULL;
					}
					else
						whichargs = 1;
				}
			}
			else {
				whichargs = 2;
			}
		}
        }

	const GValue *value = NULL;
	GdaDataModel *model = NULL;

	if (whichargs == 1) 
		value = get_table_value_at_cell (cnc, error, data, table, column, row_cond, &model);
	else if (whichargs == 2) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		if (!pname) 
			g_set_error (error, 0, 0,
				     _("No parameter named '%s' defined"), pname);
		else
			value = gda_holder_get_value (param);
	}
	else 
		g_set_error (error, 0, 0, "%s", 
                             _("Wrong number of arguments"));

	if (value) {
		/* to file through this blob */
		gboolean done = FALSE;

		if (G_VALUE_TYPE (value) == GDA_TYPE_BLOB) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			const GdaBlob *fblob = gda_value_get_blob (value);
			if (gda_blob_op_write (tblob->op, (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, 0, 0, "%s", 
					     _("Could not write file"));
			else
				done = TRUE;
			gda_value_free (vblob);
		}
		else if (G_VALUE_TYPE (value) == GDA_TYPE_BINARY) {
			GValue *vblob = gda_value_new_blob_from_file (filename);
			GdaBlob *tblob = (GdaBlob*) gda_value_get_blob (vblob);
			GdaBlob *fblob = g_new0 (GdaBlob, 1);
			const GdaBinary *fbin = gda_value_get_binary (value);
			((GdaBinary *) fblob)->data = fbin->data;
			((GdaBinary *) fblob)->binary_length = fbin->binary_length;
			if (gda_blob_op_write (tblob->op, (GdaBlob*) fblob, 0) < 0)
				g_set_error (error, 0, 0, "%s", 
					     _("Could not write file"));
			else
				done = TRUE;
			g_free (fblob);
			gda_value_free (vblob);
		}
		else {
			gchar *str;
			str = gda_value_stringify (value);
			if (g_file_set_contents (filename, str, -1, error))
				done = TRUE;
			g_free (str);
		}
		
		if (done) {
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
	}
	if (model)
		g_object_unref (model);

				
	return res;
}


static GdaInternalCommandResult *
extra_command_unset (G_GNUC_UNUSED SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
		     GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;
	const gchar *pname = NULL;

	if (args[0] && *args[0]) 
		pname = args[0];

	if (pname) {
		GdaHolder *param = g_hash_table_lookup (main_data->parameters, pname);
		if (param) {
			g_hash_table_remove (main_data->parameters, pname);
			res = g_new0 (GdaInternalCommandResult, 1);
			res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		}
		else 
			g_set_error (error, 0, 0,
				     _("No parameter named '%s' defined"), pname);
	}
	else {
		g_hash_table_destroy (main_data->parameters);
		main_data->parameters = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	}
		
	return res;
}

#define DO_UNLINK(x) g_unlink(x)
static void
graph_func_child_died_cb (GPid pid, G_GNUC_UNUSED gint status, gchar *fname)
{
	DO_UNLINK (fname);
	g_free (fname);
	g_spawn_close_pid (pid);
}

static gchar *
create_graph_from_meta_struct (G_GNUC_UNUSED GdaConnection *cnc, GdaMetaStruct *mstruct, GError **error)
{
#define FNAME "graph.dot"
	gchar *graph;

	graph = gda_meta_struct_dump_as_graph (mstruct, GDA_META_GRAPH_COLUMNS, error);
	if (!graph)
		return NULL;

	/* do something with the graph */
	gchar *result = NULL;
	if (g_file_set_contents (FNAME, graph, -1, error)) {
		const gchar *viewer;
		const gchar *format;
		
		viewer = g_getenv ("GDA_SQL_VIEWER_PNG");
		if (viewer)
			format = "png";
		else {
			viewer = g_getenv ("GDA_SQL_VIEWER_PDF");
			if (viewer)
				format = "pdf";
		}
		if (viewer) {
			static gint counter = 0;
			gchar *tmpname, *suffix;
			gchar *argv[] = {"dot", NULL, "-o",  NULL, FNAME, NULL};
			guint eventid = 0;

			suffix = g_strdup_printf (".gda_graph_tmp-%d", counter++);
			tmpname = g_build_filename (g_get_tmp_dir (), suffix, NULL);
			g_free (suffix);
			argv[1] = g_strdup_printf ("-T%s", format);
			argv[3] = tmpname;
			if (g_spawn_sync (NULL, argv, NULL,
					  G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL, 
					  NULL, NULL,
					  NULL, NULL, NULL, NULL)) {
				gchar *vargv[3];
				GPid pid;
				vargv[0] = (gchar *) viewer;
				vargv[1] = tmpname;
				vargv[2] = NULL;
				if (g_spawn_async (NULL, vargv, NULL,
						   G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL | 
						   G_SPAWN_STDERR_TO_DEV_NULL | G_SPAWN_DO_NOT_REAP_CHILD, 
						   NULL, NULL,
						   &pid, NULL)) {
					eventid = g_child_watch_add (pid, (GChildWatchFunc) graph_func_child_died_cb, 
								     tmpname);
				}
			}
			if (eventid == 0) {
				DO_UNLINK (tmpname);
				g_free (tmpname);
			}
			g_free (argv[1]);
			result = g_strdup_printf (_("Graph written to '%s'\n"), FNAME);
		}
		else 
			result = g_strdup_printf (_("Graph written to '%s'\n"
						    "Use 'dot' (from the GraphViz package) to create a picture, for example:\n"
						    "\tdot -Tpng -o graph.png %s\n"
						    "Note: set the GDA_SQL_VIEWER_PNG or GDA_SQL_VIEWER_PDF environment "
						    "variables to view the graph\n"), FNAME, FNAME);
	}
	g_free (graph);
	return result;
}

static GdaInternalCommandResult *
extra_command_graph (SqlConsole *console, GdaConnection *cnc, const gchar **args,
		     GError **error, G_GNUC_UNUSED gpointer data)
{
	gchar *result;
	GdaMetaStruct *mstruct;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!cnc) {
		g_set_error (error, 0, 0, "%s", _("No current connection"));
		return NULL;
	}

	mstruct = gda_internal_command_build_meta_struct (cnc, args, error);
	if (!mstruct)
		return NULL;
	
	result = create_graph_from_meta_struct (cnc, mstruct, error);
	g_object_unref (mstruct);
	if (result) {
		GdaInternalCommandResult *res;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (result);
		g_free (result);
		return res;
	}
	else 
		return NULL;
}

#ifdef HAVE_LIBSOUP
static GdaInternalCommandResult *
extra_command_httpd (SqlConsole *console, G_GNUC_UNUSED GdaConnection *cnc, const gchar **args,
		     GError **error, G_GNUC_UNUSED gpointer data)
{
	GdaInternalCommandResult *res = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (main_data->server) {
		/* stop server */
		g_object_unref (main_data->server);
		main_data->server = NULL;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
		res->u.txt = g_string_new (_("HTTPD server stopped"));
	}
	else {
		/* start new server */
		gint port = 12345;
		const gchar *auth_token = NULL;
		if (args[0] && *args[0]) {
			gchar *ptr;
			port = (gint) strtol (args[0], &ptr, 10);
			if (ptr && *ptr)
				port = -1;
			if (args[1] && *args[1]) {
				auth_token = args[1];
			}
		}
		if (port > 0) {
			main_data->server = web_server_new (port, auth_token);
			if (!main_data->server) 
				g_set_error (error, 0, 0, "%s", 
					     _("Could not start HTTPD server"));
			else {
				res = g_new0 (GdaInternalCommandResult, 1);
				res->type = GDA_INTERNAL_COMMAND_RESULT_TXT_STDOUT;
				res->u.txt = g_string_new (_("HTTPD server started"));
			}
		}
		else
			g_set_error (error, 0, 0, "%s", 
				     _("Invalid port specification"));
	}

	return res;
}
#endif

#ifdef NONE
static GdaInternalCommandResult *
extra_command_lo_update (SqlConsole *console, GdaConnection *cnc, const gchar **args,
			 GError **error, gpointer data)
{
	GdaInternalCommandResult *res;

	const gchar *table = NULL;
        const gchar *blob_col = NULL;
        const gchar *filename = NULL;
        const gchar *row_cond = NULL;

	if (console) {
		GdaInternalCommandResult *res;
		
		TO_IMPLEMENT;
		res = g_new0 (GdaInternalCommandResult, 1);
		res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
		return res;
	}

	if (!cnc) {
                g_set_error (error, 0, 0, "%s", _("No current connection"));
                return NULL;
        }

        if (args[0] && *args[0]) {
                filename = args[0];
                if (args[1] && *args[1]) {
                        table = args[1];
			if (args[2] && *args[2]) {
				blob_col = args[2];
				if (args[3] && *args[3]) {
					row_cond = args[3];
					if (args [4]) {
						g_set_error (error, 0, 0,
							     _("Too many arguments"));
						return NULL;
					}
				}
			}
		}
        }
	if (!row_cond) {
		g_set_error (error, 0, 0, "%s", 
                             _("Missing arguments"));
		return NULL;
	}

	g_print ("file: #%s#\n", filename);
	g_print ("table: #%s#\n", table);
	g_print ("col: #%s#\n", blob_col);
	g_print ("cond: #%s#\n", row_cond);
	TO_IMPLEMENT;

	/* prepare executed statement */
	gchar *sql;
	gchar *rtable, *rblob_col;
	
	rtable = gda_sql_identifier_quote (table, cnc, NULL, FALSE, FALSE);
	rblob_col = gda_sql_identifier_quote (blob_col, cnc, NULL, FALSE, FALSE);
	sql = g_strdup_printf ("UPDATE %s SET %s = ##blob::GdaBlob WHERE %s", rtable, rblob_col, row_cond);
	g_free (rtable);
	g_free (rblob_col);

	GdaStatement *stmt;
	const gchar *remain;
	stmt = gda_sql_parser_parse_string (main_data->current->parser, sql, &remain, error);
	g_free (sql);
	if (!stmt)
		return NULL;
	if (remain) {
		g_set_error (error, 0, 0, "%s", 
			     _("Wrong row condition"));
		return NULL;
	}

	/* prepare execution environment */
	GdaSet *params;
	GdaHolder *h;
	GValue *blob;

	if (!gda_statement_get_parameters (stmt, &params, error)) {
		g_object_unref (stmt);
		return NULL;
	}

	h = gda_set_get_holder (params, "blob");
	g_assert (h);
	blob = gda_value_new_blob_from_file (filename);
	if (!gda_holder_take_value (h, blob, error)) {
		gda_value_free (blob);
		g_object_unref (params);
		g_object_unref (stmt);
		return NULL;
	}

	/* execute statement */
	gint nrows;
	nrows = gda_connection_statement_execute_non_select (cnc, stmt, params, NULL, error);
	g_object_unref (params);
	g_object_unref (stmt);
	if (nrows == -1)
		return NULL;

	res = g_new0 (GdaInternalCommandResult, 1);
	res->type = GDA_INTERNAL_COMMAND_RESULT_EMPTY;
	return res;
}
#endif


static gchar **
args_as_string_func (const gchar *str)
{
	return g_strsplit (str, " ", 2);
}

static gchar **
args_as_string_set (const gchar *str)
{
	return g_strsplit (str, " ", 3);
}

static char **
completion_func (G_GNUC_UNUSED const char *text, int start, int end)
{
#ifdef HAVE_READLINE
	ConnectionSetting *cs = main_data->current;
	char **array;
	gchar **gda_compl;
	gint i, nb_compl;

	if (!cs)
		return NULL;
	gda_compl = gda_completion_list_get (cs->cnc, rl_line_buffer, start, end);
	if (!gda_compl)
		return NULL;

	for (nb_compl = 0; gda_compl[nb_compl]; nb_compl++);

	array = malloc (sizeof (char*) * (nb_compl + 1));
	for (i = 0; i < nb_compl; i++) {
		char *str;
		gchar *gstr;
		gint l;

		gstr = gda_compl[i];
		l = strlen (gstr) + 1;
		str = malloc (sizeof (char) * l);
		memcpy (str, gstr, l);
		array[i] = str;
	}
	array[i] = NULL;
	g_strfreev (gda_compl);
	return array;
#else
	return NULL;
#endif
}


const GSList *
gda_sql_get_all_connections (void)
{
	return main_data->settings;
}

const ConnectionSetting *
gda_sql_get_connection (const gchar *name)
{
	return find_connection_from_name (name);
}

const ConnectionSetting *
gda_sql_get_current_connection (void)
{
	return main_data->current;
}

gchar *
gda_sql_console_execute (SqlConsole *console, const gchar *command, GError **error)
{
	gchar *loc_cmde = NULL;
	gchar *retstr = NULL;

	loc_cmde = g_strdup (command);
	g_strchug (loc_cmde);
	if (*loc_cmde) {
		if (command_is_complete (loc_cmde)) {
			/* execute command */
			GdaInternalCommandResult *res;
			
			res = command_execute (console, loc_cmde, error);
			
			if (res) {
				OutputFormat of = console->output_format;
				if (res->type == GDA_INTERNAL_COMMAND_RESULT_DATA_MODEL)
					console->output_format = OUTPUT_FORMAT_HTML;

				retstr = result_to_string (console, res);
				console->output_format = of;
				gda_internal_command_exec_result_free (res);
			}
		}
		else {
			g_set_error (error, 0, 0,
				     _("Command is incomplete"));
		}
	}
	g_free (loc_cmde);

	return retstr;
}

SqlConsole *
gda_sql_console_new (const gchar *id)
{
	SqlConsole *c;

	c = g_new0 (SqlConsole, 1);
	if (id)
		c->id = g_strdup (id);
	c->current = main_data->current;
	return c;
}

void
gda_sql_console_free (SqlConsole *console)
{
	g_free (console->id);
	g_free (console);
}

gchar *
gda_sql_console_compute_prompt (SqlConsole *console)
{
	GString *string;
	gchar *str;

	string = g_string_new ("");
	compute_prompt (console, string, FALSE);

	str = string->str;
	g_string_free (string, FALSE);

	return str;
}
