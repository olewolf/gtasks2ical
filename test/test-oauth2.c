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
#include "gtasks2ical.h"
#include "oauth2.h"
#include "testfunctions.h"


#pragma GCC diagnostic ignored "-Wunused-parameter"




static void test__get_forms( const char *param );


const struct dispatch_table_t dispatch_table[ ] =
{
	DISPATCHENTRY( get_forms ),

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

	printf( "%d: action = %s\n", *counter, form->action );
	printf( "%d: name   = %s\n", *counter, form->name );
	printf( "%d: value  = %s\n", *counter, form->value );

	inputs = form->input_fields;
	g_slist_foreach( inputs, print_input, counter );

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

