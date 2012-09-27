/**
 * \file gtasks2ical.c
 * \brief Main entry for converting between iCalendar and Google Tasks.
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
#include <libxml/parser.h>
#include <curl/curl.h>
#include "gtasks2ical.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


static struct configuration_t global_config;


/**
 * The entry function decodes the command-line switches and invokes the
 * associated functions.
 * @param argc [in] The number of input arguments, used by the getopt library.
 * @param argv [in] The input argument strings, used by the getopt library.
 * @return EXIT_SUCCESS if no errors were encountered, EXIT_FAILURE otherwise.
 */
int
main( int argc, char **argv )
{
	CURL *curl;

	/* Initialize libxml. */
	LIBXML_TEST_VERSION;

	/* Initialize CURL. */
	curl_global_init( CURL_GLOBAL_ALL );
	curl = curl_easy_init( );
	curl_easy_setopt( curl, CURLOPT_COOKIEFILE, "" );
	curl_easy_setopt( curl, CURLOPT_COOKIESESSION, 1 );

	/* Setup command-line options and environment variable options. */
    initialize_configuration( &global_config, argc, (const char *const *)argv );

/*
	Google_Authenticate( "username", "password" );
*/


	curl_global_cleanup( );
	xmlCleanupParser( );

	exit( 0 );
}
