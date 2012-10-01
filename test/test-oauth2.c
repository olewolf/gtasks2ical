/**
 * \file test-oauth2.c
 * \brief Test the oauth2 functions.
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
#include <stdio.h>
#include <glib.h>
#include <unistd.h>
#include <string.h>
#include <curl/curl.h>
#include "gtasks2ical.h"
#include "oauth2-google.h"
#include "postform.h"
#include "testfunctions.h"


#pragma GCC diagnostic ignored "-Wunused-parameter"


struct configuration_t global_config;

extern int search_input_by_name( gconstpointer input_ptr,
								 gconstpointer input_names_ptr );
extern GSList *get_forms( const gchar *raw_html_page );
extern int search_form_by_action_and_inputs( gconstpointer form_ptr,
											 gconstpointer search_ptr );
extern form_field_t *find_form( const gchar *raw_html_page,
								const gchar *form_action,
								const gchar *const *input_names );
extern size_t receive_curl_response( const char *ptr, size_t size,
									 size_t nmemb, void *state_data );
extern gchar *read_url( CURL *curl, const gchar *url );
extern form_field_t *get_form_from_url( CURL* curl, const gchar *url,
										const gchar *form_action,
										const gchar *const *input_names );
extern void modify_form_inputs( gpointer input_ptr, gpointer form_ptr );
extern void modify_form( form_field_t *form, const GSList *inputs_to_modify );
extern gchar *post_form( CURL *curl, const form_field_t *form,
						 struct curl_slist *headers );


static void test__get_forms( const char *param );
static void test__search_input_by_name( const char *param );
static void test__search_form_by_action_and_inputs( const char *param );
static void test__find_form( const char *param );
static void test__receive_curl_response( const char *param );
static void test__read_url( const char *param );
static void test__get_form_from_url( const char *param );
static void test__modify_form_inputs( const char *param );
static void test__modify_form( const char *param );
static void test__post_form( const char *param );
static void test__login_to_gmail( const char *param );


const struct dispatch_table_t dispatch_table[ ] =
{
	DISPATCHENTRY( get_forms ),
	DISPATCHENTRY( search_input_by_name ),
	DISPATCHENTRY( search_form_by_action_and_inputs ),
	DISPATCHENTRY( find_form ),
	DISPATCHENTRY( receive_curl_response ),
	DISPATCHENTRY( read_url ),
	DISPATCHENTRY( get_form_from_url ),
	DISPATCHENTRY( modify_form_inputs ),
	DISPATCHENTRY( modify_form ),
	DISPATCHENTRY( post_form ),
	DISPATCHENTRY( login_to_gmail ),

    { NULL, NULL }
};



static void print_input( gpointer input_contents, gpointer ctr_ptr )
{
	input_field_t *input   = input_contents;
	int           *counter = ctr_ptr;

	printf( "input %d: name = %s, value = %s\n", *counter,
			input->name, input->value );
}

static void print_form( gpointer form_contents, gpointer ctr_ptr )
{
	form_field_t *form    = form_contents;
	int          *counter = ctr_ptr;
	GSList       *inputs;

	if( form != NULL )
	{
		printf( "%d: action = %s\n", *counter, form->action );
		printf( "%d: name   = %s\n", *counter, form->name );
		printf( "%d: value  = %s\n", *counter, form->value );

		inputs = form->input_fields;
		g_slist_foreach( inputs, print_input, counter );
	}

	(*counter)++;
}


static void test__get_forms( const char *param )
{
	FILE *filehd;
	char *html_buf;
	GSList *forms;
	int    ctr;

	html_buf = malloc( 75000 );

	filehd = fopen( "../../data/oauth2-confirm.html", "r" );
	if( ! filehd )
	{
		printf( "Error: cannot find test data\n" );
		exit( EXIT_FAILURE );
	}
	if( fread( html_buf, 75000, 1, filehd ) )
	{
		printf( "Error: cannot read test data\n" );
		exit( EXIT_FAILURE );
	}

	forms = get_forms( html_buf );
	ctr = 1;
	g_slist_foreach( forms, print_form, &ctr );
}



static void test__search_input_by_name( const char *param )
{
	gchar         *input_names[ ] = { "name1", "name2", "name3", NULL };
	input_field_t input1          = { .name = "name2", .value = "value2" };
	input_field_t input2          = { .name = "name3", .value = "value3" };
	input_field_t input4          = { .name = "name4", .value = "value4" };
	int found;

	found = search_input_by_name( &input1, input_names );
	printf( "1: %d\n", found );
	found = search_input_by_name( &input2, input_names );
	printf( "2: %d\n", found );
	found = search_input_by_name( &input4, input_names );
	printf( "3: %d\n", found );
}



static void test__search_form_by_action_and_inputs( const char *param )
{
	input_field_t input1 = { "in1", "ivalue1" };
	input_field_t input2 = { "in2", "ivalue2" };
	input_field_t input3 = { "in3", "ivalue3" };
	GSList        *input;

	form_field_t form1 = { "form1", "value1", "action1", NULL };

	const gchar *search1[ ] = { "in1", NULL };
	const gchar *search2[ ] = { "in4", NULL };
	const gchar *search3[ ] = { "in2", NULL };
	struct form_search_t search_form1 = { "action1", search1 };
	struct form_search_t search_form2 = { "act", search2 };
	struct form_search_t search_form3 = { "action4", search3 };

	int found;

	input = g_slist_append( NULL, &input1 );
	input = g_slist_append( input, &input2 );
	input = g_slist_append( input, &input3 );

	form1.input_fields = input;
	found = search_form_by_action_and_inputs( &form1, &search_form1 );
	printf( "1: %d\n", found );
	found = search_form_by_action_and_inputs( &form1, &search_form2 );
	printf( "2: %d\n", found );
	found = search_form_by_action_and_inputs( &form1, &search_form3 );
	printf( "3: %d\n", found );
}



static void test__find_form( const char *param )
{
	FILE *filehd;
	char *html_buf;
	int    ctr;
	form_field_t *form1;
	form_field_t *form2;
	const char *input_names[ ] = { "bgresponse", "submit_access", NULL };

	html_buf = malloc( 75000 );

	filehd = fopen( "../../data/oauth2-confirm.html", "r" );
	if( ! filehd )
	{
		printf( "Error: cannot find test data\n" );
		exit( EXIT_FAILURE );
	}
	if( fread( html_buf, 75000, 1, filehd ) )
	{
		printf( "Error: cannot read test data\n" );
		exit( EXIT_FAILURE );
	}

	ctr = 1;
	form1 = find_form( html_buf, "https://accounts.google.com/", input_names );
	print_form( form1, &ctr );
	form2 = find_form( html_buf, "https://accounts.google.com/o/oauth2/nonexistent", input_names );
	if( form2 != NULL )
	{
		printf( "2: found\n" );
	}
	else
	{
		printf( "2: not found\n" );
	}
}



static void test__receive_curl_response( const char *param )
{
	struct curl_write_buffer_t write_buffer = { NULL, 0 };
	const char                 *testdata = "01234567891011121314151617181920";
	size_t                     processed;

	processed = receive_curl_response( testdata, 5, 3, &write_buffer );
	printf( "1: %2d \"%s\"\n", (int) processed, write_buffer.data );

	processed = receive_curl_response( &testdata[ 15 ], 10, 1, &write_buffer );
	printf( "2: %2d \"%s\"\n", (int) processed, write_buffer.data );

	processed = receive_curl_response( &testdata[ 25 ], 1, 3, &write_buffer );
	printf( "3: %2d \"%s\"\n", (int) processed, write_buffer.data );
}



static void test__read_url( const char *param )
{
	CURL  *curl;
	gchar *html;

	/* Skip test if no internet access is available. */
	if( test_check_internet( ) == FALSE )
	{
		exit( 77 );
	}

	curl = curl_easy_init( );
	html = read_url( curl, "http://www.microsoft.com" );
	curl_easy_cleanup( curl );
	if( html != NULL )
	{
		printf( "%s\n", html );
		g_free( html );
	}
}



static void test__get_form_from_url( const char *param )
{
	CURL *curl;
	int  ctr;
	char login_url[ 2048 ] = "file://";

	const gchar *action = "https://accounts.google.com/ServiceLoginAuth";
	const gchar *const input_names[ ] = { "Email", "Passwd", NULL };
	form_field_t *form;

	/* Create a file URL. */
	getcwd( &login_url[ strlen( "file://" ) ], sizeof( login_url ) );
	strcat( login_url, "/../../data/google-login.html" );

	curl = curl_easy_init( );
	form = get_form_from_url( curl, login_url, action, input_names );
	curl_easy_cleanup( curl );

	ctr = 1;
	print_form( form, &ctr );
}



static void test__modify_form_inputs( const char *param )
{
	FILE               *filehd;
	char               *html_buf;
	const gchar *const input_names[ ] = { NULL, NULL };
	form_field_t       *form;
	int                ctr = 1;

	const input_field_t input_to_change1 = { .name = "dsh", .value = "test1" };
	const input_field_t input_to_change2 = { .name = "new", .value = "test2" };

	html_buf = malloc( 75000 );
	filehd = fopen( "../../data/google-login.html", "r" );
	if( ! filehd )
	{
		printf( "Error: cannot find test data\n" );
		exit( EXIT_FAILURE );
	}
	if( fread( html_buf, 75000, 1, filehd ) )
	{
		printf( "Error: cannot read test data\n" );
		exit( EXIT_FAILURE );
	}
	form = find_form( html_buf, "https://accounts.google.com/", input_names );
	if( ! form )
	{
		printf( "Form not found\n" );
		exit( 1 );
	}

	modify_form_inputs( (gpointer) &input_to_change1, (gpointer) form );
	modify_form_inputs( (gpointer) &input_to_change2, (gpointer) form );
	print_form( form, &ctr );
}



static void test__modify_form( const char *param )
{
	FILE               *filehd;
	char               *html_buf;
	const gchar *const input_names[ ] = { NULL, NULL };
	form_field_t       *form;
	int                ctr = 1;

	input_field_t input_to_change1  = { .name = "dsh", .value = "test1" };
	input_field_t input_to_change2  = { .name = "new", .value = "test2" };
	GSList        *inputs_to_modify = NULL;

	html_buf = malloc( 75000 );
	filehd = fopen( "../../data/google-login.html", "r" );
	if( ! filehd )
	{
		printf( "Error: cannot find test data\n" );
		exit( EXIT_FAILURE );
	}
	if( fread( html_buf, 75000, 1, filehd ) )
	{
		printf( "Error: cannot read test data\n" );
		exit( EXIT_FAILURE );
	}
	form = find_form( html_buf, "https://accounts.google.com/", input_names );
	if( ! form )
	{
		printf( "Form not found\n" );
		exit( 1 );
	}

	inputs_to_modify = g_slist_append( inputs_to_modify, &input_to_change1 );
	inputs_to_modify = g_slist_append( inputs_to_modify, &input_to_change2 );
	modify_form( form, inputs_to_modify );

	print_form( form, &ctr );
}



static void test__post_form( const char *param )
{
	FILE               *filehd;
	char               *html_buf;
	const gchar *const input_names[ ] = { NULL, NULL };
	form_field_t       *form;
	char               action_url[ 2048 ] = "file://";
	CURL               *curl;
	char               *html;

	html_buf = malloc( 75000 );
	filehd = fopen( "../../data/google-login.html", "r" );
	if( ! filehd )
	{
		printf( "Error: cannot find test data\n" );
		exit( EXIT_FAILURE );
	}
	if( fread( html_buf, 75000, 1, filehd ) )
	{
		printf( "Error: cannot read test data\n" );
		exit( EXIT_FAILURE );
	}
	form = find_form( html_buf, "https://accounts.google.com/", input_names );
	if( ! form )
	{
		printf( "Form not found\n" );
		exit( 1 );
	}

	/* Create a file URL action. */
	g_free( form->action );
	getcwd( &action_url[ strlen( "file://" ) ], sizeof( action_url ) );
	strcat( action_url, "/../../data/post.response.html" );
	form->action = action_url;

	curl = curl_easy_init( );
	html = post_form( curl, form, NULL );
	curl_easy_cleanup( curl );
	printf( "%s\n", html );
}



static void test__login_to_gmail( const char *param )
{
	FILE *filehd;
	char username[ 100 ];
	char password[ 100 ];
	CURL *curl;

	global_config.ipv4_only = TRUE;

	/* Skip test if no internet access is available. */
	if( test_check_internet( ) == FALSE )
	{
		exit( 77 );
	}

	/* Skip the test if the password file is not available. To enable the
	   test, enter your own email and password in the "password" file in the
	   test/data directory; use test/data/password.template as a template. */
	filehd = fopen( "../../data/password", "r" );
	if( ! filehd )
	{
		exit( 77 );
	}
	fgets( username, sizeof( username ), filehd );
	fgets( password, sizeof( username ), filehd );

	curl = curl_easy_init( );
	curl_easy_setopt( curl, CURLOPT_COOKIESESSION, 1 );
	curl_easy_setopt( curl, CURLOPT_COOKIEFILE, "cookies.txt" );
	(void)login_to_gmail( curl, username, password );
	curl_easy_cleanup( curl );
}
