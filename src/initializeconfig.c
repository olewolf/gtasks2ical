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
#include "gtasks2ical.h"



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
	configuration->verbose             = FALSE;
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
		{ "task",       required_argument, NULL, (int)'t' },
		{ "download",   no_argument,       NULL, (int)'d' },
		{ "upload",     no_argument,       NULL, (int)'u' },
        { "help",       no_argument,       NULL, (int)'h' },
        { "version",    no_argument,       NULL, (int)'V' },
        { "license",    no_argument,       NULL, (int)'L' },
        { "verbose",    no_argument,       NULL, (int)'v' },
		{ NULL, 0, NULL, 0 }
    };
    int      option_character;
    int      option_index = 0;
    gboolean option_error = FALSE;
    gint     remaining_arguments;


    /* Decode the command-line switches. */
    while( ( option_character = getopt_long( argc, (char * const *) argv,
											 "hVLvdut:", long_options,
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

		/* Specify specific task to be synchronized. */
		case 't':
			configuration->tasks = g_slist_append( configuration->tasks,
												   g_strdup( argv[ optind ]) );
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

    assert( configuration != NULL );

	/* Initialize the configuration to default values. */
	reset_configuration( configuration );

	/* Decode the command-line options and arguments. */
	success = decode_commandline( configuration, argc, argv );
	if( success == TRUE )
	{
		success = set_environment_configuration( configuration );
	}

	return( success );
}
