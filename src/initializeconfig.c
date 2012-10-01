/**
 * \file initializeconfig.c
 * \brief Decode the command line, setting configurations or printing info.
 *
 * Copyright (C) 2012 Ole Wolf <wolf@blazingangles.com>
 *
 * This file is part of gtasks2ical.
 * 
 * gtasks2ical is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <assert.h>
#include <glib.h>
#include <glib/gprintf.h>
#include <glib/gkeyfile.h>
#include "gtasks2ical.h"


#define STRINGIFY(x) XSTRINGIFY(x)
#define XSTRINGIFY(x) #x


/**
 * Print the version of this software and exit.
 * @param command_name [in] The filename of this file's executable.
 * @return Nothing.
 */
static void
print_software_version( const gchar *command_name )
{
    printf( "%s version %s\n", command_name, VERSION );
}



/**
 * Print a help screen for this software and exit.
 * @param command_name [in] The filename of this file's executable.
 * @param show_options [in] \a true if the options should be displayed, or
 *        \a false if only the usage summary should be displayed.
 * @return Nothing.
 */
static void
print_software_help( const gchar *command_name, gboolean show_options )
{
    printf( "Usage:\n    %s [options] [listname] file|directory\n\n",
			command_name );

    if( show_options == TRUE )
    {
        printf( "Options:\n\n"
				"      -d, --download        Force download instead of synchronizing\n"
				"      -u, --upload          Force download instead of synchronizing\n"
				"      -t EID, --task=EID    Process only the task with the specified EID. This\n"
				"                            option may be specified multiple times.\n"
				"      -c file,              Read configuration settings from \"file\", over-\n"
				"      --config=file         riding any settings that were applied by the\n"
                "                            system-wide configuration file and the local\n"
				"                            configuration file.\n"
				"      -4, --ipv4only        Force IPv4, disabling IPv6.\n"
				"\n"
				"      -h, --help            Print this help and exit\n"
				"      -V, --version         Print version and exit\n"

				"      -v, --verbose         Generate verbose output\n"
				"      -L, --license         Print licensing information and exit\n"
				"\nFor further help, see the man page for gtasks2ical(1).\n" );
    }
}



/**
 * Print license information for this software.
 * @param command_name [in] The filename of this file's executable.
 * @return Nothing.
 */
static void
print_software_license( const gchar *command_name )
{
    printf( "%s Copyright (C) 2012 Ole Wolf\n", command_name );

    printf( "This program comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n"
			"are welcome to redistribute it under the conditions of the GNU General Public\n"
			"License. See <http://www.gnu.org/licenses/> for details.\n\n" );
}



/**
 * Initialize a configuration structure to defined, invalid values.
 * @param configuration [out] Configuration to initialize.
 * @return Nothing.
 */
static void
reset_configuration( struct configuration_t *configuration )
{
	/* Set default options. */
	configuration->listname            = NULL;
	configuration->ical_filename       = NULL;
	configuration->force_download      = FALSE;
	configuration->force_upload        = FALSE;
	configuration->tasks               = NULL;
	configuration->gmail_username      = NULL;
	configuration->gmail_password      = NULL;
	configuration->client_id           = NULL;
	configuration->client_password     = NULL;
	configuration->verbose             = FALSE;
	configuration->ipv4_only           = FALSE;
	configuration->configuration_file  = NULL;
}



/**
 * Auxiliary function used by \a destroy_configuration to delete the contents
 * of a GSList.
 * @param data_ptr [in] Pointer to list data to destroy.
 * @param user_data_ptr [in] Unused.
 * @return Nothing.
 */
static void
destroy_config_task( gpointer data_ptr, gpointer user_data_ptr )
{
	gchar *task_id = (gchar*) data_ptr;
	g_free( task_id );
}



/**
 * Free all allocated memory in a configuration structure.  Its contents are
 * then undefined.
 * @param configuration [out] Configuration to destroy.
 * @return Nothing.
 */
static void
destroy_configuration( struct configuration_t *configuration )
{
	/* Free all allocations. */
	g_free( configuration->listname );
	g_free( configuration->ical_filename );
	g_free( configuration->gmail_username );
	g_free( configuration->gmail_password );
	g_free( configuration->client_id );
	g_free( configuration->client_password );
	g_free( configuration->configuration_file );
	g_slist_foreach( configuration->tasks, destroy_config_task, NULL );
	g_free( configuration->tasks );
}



/**
 * Decode a command line options and parameters using the getopt library. Fill
 * a \a configuration structure with options and arguments.
 * @param configuration [out] Gets filled with the configuration based on
 *                            the command line options.
 * @param argc [in] \a argc from the \a main function.
 * @param argv [in] \a argv from the \a main function.
 * @return \a true if the command line was decoded without errors, or \a false
 *         otherwise.
 */
static gboolean
decode_commandline(
    struct configuration_t *configuration, int argc, const char *const *argv )
{
    /* Use the stdopt library to parse long and short options. */
    static struct option long_options[ ] =
    {
		{ "config",     required_argument, NULL, (int)'c' },
		{ "task",       required_argument, NULL, (int)'t' },
		{ "download",   no_argument,       NULL, (int)'d' },
		{ "upload",     no_argument,       NULL, (int)'u' },
        { "help",       no_argument,       NULL, (int)'h' },
        { "version",    no_argument,       NULL, (int)'V' },
        { "license",    no_argument,       NULL, (int)'L' },
        { "verbose",    no_argument,       NULL, (int)'v' },
        { "ipv4only",   no_argument,       NULL, (int)'4' },
		{ NULL, 0, NULL, 0 }
    };
    int      option_character;
    int      option_index = 0;
    gboolean option_error = FALSE;
    gint     remaining_arguments;


    /* Decode the command-line switches. */
    while( ( option_character = getopt_long( argc, (char * const *) argv,
											 "hVLvdut:c:4", long_options,
											 &option_index ) ) != -1 )
    {
        switch( option_character )
		{
		/* End of options. */
		case 0:
			break;

		/* Print help information. */
		case 'h':
			print_software_help( argv[ 0 ], TRUE );
			exit( EXIT_SUCCESS );

		/* Print version information. */
		case 'V':
			print_software_version( argv[ 0 ] );
			exit( EXIT_SUCCESS );

		/* Print licensing information. */
		case 'L':
			print_software_license( argv[ 0 ] );
			exit( EXIT_SUCCESS );

		/* Set verbosity. */
		case 'v':
			configuration->verbose = TRUE;
			break;

		/* Force download. */
		case 'd':
			configuration->force_download = TRUE;
			break;

		/* Force upload. */
		case 'u':
			configuration->force_download = TRUE;
			break;

		/* Disable IPv6. */
		case '4':
			configuration->ipv4_only = TRUE;
			break;

		/* Specify specific task to be synchronized. */
		case 't':
			configuration->tasks = g_slist_append( configuration->tasks,
												   g_strdup( argv[ optind ] ) );
			break;

		/* Specify specific task to be synchronized. */
		case 'c':
			g_free( configuration->configuration_file );
			configuration->configuration_file = g_strdup( argv[ optind ] );
			break;

		case '?':
			option_error = TRUE;
			break;

		default:
			abort( );
		}
    }

    /* Show the help screen if an error is encountered. */
    if( option_error )
    {
        print_software_help( argv[ 0 ], FALSE );
		exit( EXIT_FAILURE );
    }

    /* Parse command line arguments (not options). */
    remaining_arguments = argc - optind;
	/* Set the Google Tasks list name, if specified. */
	if( remaining_arguments == 2 )
	{
		configuration->listname      = g_strdup( argv[ optind++ ] );
		configuration->ical_filename = g_strdup( argv[ optind ] );
	}
	/* Only the local file/dir is specified. */
	else if( remaining_arguments == 1 )
	{
		configuration->ical_filename = g_strdup( argv[ optind ] );
	}
	else
    {
        print_software_help( argv[ 0 ], FALSE );
		option_error = TRUE;
    }

	/* Print verbose output, if enabled. */
	if( option_error == FALSE )
	{
		if( configuration->verbose )
		{
			g_printf( "iCalendar file/directory: %s\n",
					  configuration->ical_filename );
			g_printf( "Google Tasks task lists: " );
			if( configuration->listname != NULL )
			{
				g_printf( "regexp = \"%s\"\n", configuration->listname );
			}
			else
			{
				g_printf( "Using default list\n" );
			}
		}
	}

    return( ! option_error );
}



/**
 * Initialize the configuration based on the specified configuration file.
 * @param configuration [out] Configuration that is initialized based on the
 *                      configuration file contents.
 * @param configuration_file [in] Configuration file that should be applied.
 * @return \a TRUE if the configuration was successfully initialized.  (This
 *         may also happen if no default configuration files are available
 *         and no other configuration file is specified).
 */
static gboolean
apply_one_configuration_file( const gchar            *configuration_file,
							  struct configuration_t *configuration )
{
	GKeyFile    *key_file;
	GError      *file_error = NULL;
	gboolean    success;
	gchar       *key_group;
	gchar       **keys;
	gint        key_idx;
	const gchar *key_name;
	gchar       *client_id;
	gchar       *client_password;
	gchar       *username;
	gchar       *password;
	gboolean    ipv4_only;

	/* Return reporting success if the configuration file is not specified. */
	if( configuration_file == NULL )
	{
		success = TRUE;
	}
	else
	{
		/* Read the configuration file. */
		key_file = g_key_file_new( );
		success = g_key_file_load_from_file( key_file, configuration_file,
											 G_KEY_FILE_NONE, &file_error );
		/* Apply the settings from the configuration file. */
		if( success == TRUE )
		{
			key_group = g_key_file_get_start_group( key_file );
			keys      = g_key_file_get_keys( key_file, key_group, NULL, NULL );
			key_idx   = 0;
			/* Apply the setting for each key. */
			while( ( key_name = keys[ key_idx++ ] ) != NULL )
			{
				/* Set the Client ID. */
				if( g_strcmp0( key_name, "client id" ) == 0 )
				{
					client_id = g_key_file_get_string( key_file, key_group,
													   key_name, NULL );
					if( client_id != NULL )
					{
						g_free( configuration->client_id );
						configuration->client_id = client_id;
					}
				}
				/* Set the Client password. */
				if( g_strcmp0( key_name, "client secret" ) == 0 )
				{
					client_password = g_key_file_get_string( key_file,
															 key_group,
															 key_name, NULL );
					if( client_password != NULL )
					{
						g_free( configuration->client_password );
						configuration->client_password = client_password;
					}
				}
				/* Set the username. */
				else if( g_strcmp0( key_name, "gmail user" ) == 0 )
				{
					username = g_key_file_get_string( key_file, key_group,
													  key_name, NULL );
					if( username != NULL )
					{
						g_free( configuration->gmail_username );
						configuration->gmail_username = username;
					}
				}
				/* Set the password. */
				else if( g_strcmp0( key_name, "gmail password" ) == 0 )
				{
					password = g_key_file_get_string( key_file, key_group,
													  key_name, NULL );
					if( password != NULL )
					{
						g_free( configuration->gmail_password );
						configuration->gmail_password = password;
					}
				}
				/* Enable or disable IPv6. */
				else if( g_strcmp0( key_name, "ipv4 only" ) == 0 )
				{
					ipv4_only = g_key_file_get_boolean( key_file, key_group,
														key_name, NULL );
					configuration->ipv4_only = ipv4_only;
				}
			}
			g_strfreev( keys );
		}
		/* Report the error unless it is caused by insufficient permissions or
		   the file does not exist, in which case the file is ignored. */
		else
		{
			if( ( file_error->code != G_FILE_ERROR_ACCES         ) &&
				( file_error->code != G_FILE_ERROR_NOENT         ) &&
				( file_error->code != G_KEY_FILE_ERROR_NOT_FOUND ) )
			{
				fprintf( stderr, "Error: %s\n", file_error->message );
				success = FALSE;
			}
			else
			{
				success = TRUE;
			}
		}
	}

	return( success );
}



/**
 * Read and apply three files successively, overwriting any settings that
 * were initialized by the previous read, in the following order:
 * /etc/gtasks2ical.conf, ~/.gtasks2icalrc, and any file specified as a
 * command-line option.
 * @param custom_conf_file [in] Configuration file specified as a command-
 *        line option.  Ignored if \a NULL.
 * @param configuration [out] Configuration settings based on the contents of
 *        the configuration files.
 * @return \a TRUE if the configurations were applied successfully, or \a FALSE
 *         otherwise.
 */
static gboolean
apply_configuration_files( const gchar            *custom_conf_file,
						   struct configuration_t *configuration )
{
	gboolean    success;
	gchar       **environment;
	const gchar *user_home;
	gchar       *local_conf_filename;

	/* Apply the global configuration. */
	success = apply_one_configuration_file( STRINGIFY( SYSCONFDIR )
											STRINGIFY( CONF_FILE_NAME ),
		                                    configuration );
	if( success == TRUE )
	{
		/* Build the path of the user's local configuration file. */
		environment = g_get_environ( );
		user_home   = g_environ_getenv( environment, "HOME" );
		if( user_home == NULL )
		{
			user_home = g_get_home_dir( );
		}
		local_conf_filename = g_strconcat( user_home,
										   STRINGIFY( LOCAL_CONF_FILE_NAME ),
										   NULL );
		g_strfreev( environment );
		/* Override with the user's local configuration. */
		success = apply_one_configuration_file( local_conf_filename,
												configuration );
		g_free( local_conf_filename );
		if( success == TRUE )
		{
			/* Override with settings from the custom configuration file. */
			success = apply_one_configuration_file( custom_conf_file,
													configuration );
		}
	}

	return( success );
}



/**
 * Read the Gmail credentials from the environment variable defined in the
 * header file, and store the credentials in the configuration structure.
 * @param configuration [in/out] Configuration structure where the Gmail
 *        username and password are stored.
 * @return \a TRUE if the credentials were read successfully, or \a FALSE
 *         otherwise.
 */
static gboolean
set_environment_configuration( struct configuration_t *configuration )
{
	gchar **environment;
	const gchar *gmail_credentials;
	const gchar *cred_regex_src =
		"^\\s([a-z0-9_\\.]+@gmail.com)\\s:\\s(.+)\\s$";
	GRegex      *cred_regex;
	GMatchInfo  *match_info;
	gboolean    success;

	/* Read the environment variable with the Gmail credentials. */
	environment = g_get_environ( );
	gmail_credentials = g_environ_getenv( environment, GMAIL_CREDENTIALS );
	/* Grep the username and password. */
	if( gmail_credentials != NULL )
	{
		cred_regex = g_regex_new( cred_regex_src, G_REGEX_CASELESS,
								  G_REGEX_MATCH_NOTEMPTY, NULL );
		success = g_regex_match( cred_regex, gmail_credentials,
								 0, &match_info );
		/* Save the username and password in the configuration structure. */
		if( success == TRUE )
		{
			configuration->gmail_username = g_match_info_fetch( match_info, 1 );
			configuration->gmail_password = g_match_info_fetch( match_info, 2 );
		}
		else
		{
			configuration->gmail_username = NULL;
			configuration->gmail_password = NULL;
		}
		/* Release memory. */
		g_match_info_free( match_info );
		g_regex_unref( cred_regex );
	}
	/* It is considered an error if the environment variable was not
	   set. */
	else
	{
		success = FALSE;
	}

	return( success );
}



/**
 * Initialize the configuration according to environment variables, command-line
 * options and arguments, and configuration file.
 * @param configuration [out] Configuration structure that is initialized with
 *        user and system settings.
 * @param argc [in] \a argc from the \a main function.
 * @param argv [in] \a argv from the \a main function.
 * @return \a TRUE if the configuration was initialized successfully, or
 *         \a FALSE otherwise.
 */
gboolean
initialize_configuration(
    struct configuration_t *configuration, int argc, const char *const *argv )
{
	gboolean success;
	gchar    *configuration_file;

    assert( configuration != NULL );

	/* Initialize the configuration to default values. */
	reset_configuration( configuration );

	/* Decode the command-line options, but only to determine which
	   configuration file to use. */
	success = decode_commandline( configuration, argc, argv );
	if( success == TRUE )
	{
		/* Destroy the configuration then initialize it according to the
		   configuration file contents. */
		configuration_file = NULL;
		if( configuration->configuration_file != NULL )
		{
			configuration_file = g_strdup( configuration->configuration_file );
		}
		destroy_configuration( configuration );
		reset_configuration( configuration );
		/* Initialize the configuration with the configuration files
		   settings. */
		success = apply_configuration_files( configuration_file,
											 configuration );
		if( success == TRUE )
		{
			/* Override with environment variables.*/
			success = set_environment_configuration( configuration );
			/* Override with command-line options and arguments (which we
			   now know will happen successfully). */
			optind = 1;
			(void) decode_commandline( configuration, argc, argv );
		}
	}

	return( success );
}
