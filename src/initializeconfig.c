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
 * @param commandName [in] The filename of this file's executable.
 * @return Nothing.
 */
static void
PrintSoftwareVersion( const gchar *commandName )
{
    printf( "%s version %s\n", commandName, VERSION );
}



/**
 * Print a help screen for this software and exit.
 * @param commandName [in] The filename of this file's executable.
 * @param showOptions [in] \a true if the options should be displayed, or
 *        \a false if only the usage summary should be displayed.
 * @return Nothing.
 */
static void
PrintSoftwareHelp( const gchar *commandName, gboolean showOptions )
{
    printf( "Usage:\n    %s [options] [listname] [ical-file]\n\n",
			commandName );

    if( showOptions )
    {
        printf( "Options:\n" );
		printf( "      -h, --help            Print this help and exit\n" );
		printf( "      -V, --version         Print version and exit\n" );

		printf( "      -v, --verbose         Generate verbose output\n" );
		printf( "      -L, --license         Print licensing information and exit\n" );
		printf( "\nFor further help, see the man page for gtasks2ical(1).\n" );
    }
}



/**
 * Print license information for this software.
 * @param commandName [in] The filename of this file's executable.
 * @return Nothing.
 */
static void
PrintSoftwareLicense( const gchar *commandName )
{
    printf( "%s Copyright (C) 2012 Ole Wolf\n", commandName );

    printf( "This program comes with ABSOLUTELY NO WARRANTY. This is free software, and you\n" );
    printf( "are welcome to redistribute it under the conditions of the GNU General Public\n" );
    printf( "License. See <http://www.gnu.org/licenses/> for details.\n\n" );
}



/**
 * Decode a command line options and parameters using the getopt library. Fill
 * a \a Configuration structure with options and arguments.
 * @param configuration [out] Gets filled with the configuration based on
 *                            the command line options.
 * @param argc [in] \a argc from the \a main function.
 * @param argv [in] \a argv from the \a main function.
 * @return \a true if the command line was decoded without errors, or \a false
 *         otherwise.
 */
gboolean
Initialize_Configuration(
    struct Configuration *configuration, int argc, const char *const *argv )
{
    /* Use the stdopt library to parse long and short options. */
    static struct option longOptions[ ] =
    {
        { "help",       no_argument,       NULL, (int)'h' },
        { "version",    no_argument,       NULL, (int)'V' },
        { "license",    no_argument,       NULL, (int)'L' },
        { "verbose",    no_argument,       NULL, (int)'v' },
		{ "output",     required_argument, NULL, (int)'o' },
		{ "input",      required_argument, NULL, (int)'i' },
		{ NULL, 0, NULL, 0 }
    };
    int      optionCharacter;
    int      optionIndex = 0;
    gboolean optionError = FALSE;

    gint     remainingArguments;

    assert( configuration != NULL );

    /* If called without arguments, print the help screen and exit. */
    if( argc == 1 )
    {
        PrintSoftwareHelp( argv[ 0 ], TRUE );
		exit( EXIT_SUCCESS );
    }

	/* Set default options. */
	configuration->listname            = NULL;
	configuration->in_ical_filename    = NULL;
	configuration->out_ical_filename   = NULL;
	configuration->google_username     = NULL;
	configuration->google_password     = NULL;
	configuration->verbose             = FALSE;

    /* Decode the command-line switches. */
    while( ( optionCharacter = getopt_long( argc, (char * const *) argv,
											"hVLvi:o:", longOptions,
											&optionIndex ) ) != -1 )
    {
        switch( optionCharacter )
		{
		/* End of options. */
		case 0:
			break;

		/* Print help information. */
		case 'h':
			PrintSoftwareHelp( argv[ 0 ], TRUE );
			exit( EXIT_SUCCESS );

		/* Print version information. */
		case 'V':
			PrintSoftwareVersion( argv[ 0 ] );
			exit( EXIT_SUCCESS );

		/* Print licensing information. */
		case 'L':
			PrintSoftwareLicense( argv[ 0 ] );
			exit( EXIT_SUCCESS );

		/* Set verbosity. */
		case 'v':
			configuration->verbose = TRUE;
			break;

		/* Set input ical filename. */
		case 'i':
			configuration->in_ical_filename = g_strdup( optarg );
			break;

		/* Set output ical filename. */
		case 'o':
			configuration->out_ical_filename = g_strdup( optarg );
			break;

		case '?':
			optionError = TRUE;
			break;

		default:
			abort( );
		}
    }

    /* Show the help screen if an error is encountered. */
    if( optionError )
    {
        PrintSoftwareHelp( argv[ 0 ], FALSE );
		exit(EXIT_FAILURE );
    }

    /* Parse command line arguments (not options). */
    remainingArguments = argc - optind;
	/* Must have exactly one argument. */
    if( 1 != remainingArguments )
    {
        PrintSoftwareHelp( argv[ 0 ], FALSE );
		exit(EXIT_FAILURE );
    }
	/* Set the Google Tasks list name. */
	configuration->listname = g_strdup( argv[ optind ] );

	/* Print verbose output, if enabled. */
	if( configuration->verbose )
	{
		if( configuration->out_ical_filename != NULL )
		{
			g_printf( "Output iCal file: %s\n",
					  configuration->out_ical_filename );
		}
		if( configuration->in_ical_filename != NULL )
		{
			g_printf( "Input iCal file: %s\n",
					  configuration->in_ical_filename );
		}
		g_printf( "Google Tasks list regexp: %s\n", configuration->listname );
	}

    return( ! optionError );
}

