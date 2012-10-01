/**
 * \file postform.c
 * \brief Send HTTP POST and receive replies.
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
#include <glib.h>
#include <json-glib/json-glib.h>
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <string.h>
#include <glib/gprintf.h>
#include "gtasks2ical.h"
#include "postform.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


/* Global configuration data. */
extern struct configuration_t global_config;



/**
 * Callback function for a CURL write.  The function maintains a dynamically
 * expanding buffer that may be filled successively by multiple callbacks.
 * @param ptr [in] Source of the data from CURL.
 * @param size [in] The size of each data block.
 * @param nmemb [in] Number of data blocks.
 * @param state_data [in] Passed from the CURL write-back as a pointer to a
 *        \a struct \a curl_write_buffer_t.  It must be initialized to
 *        all-zeroes before use.
 * @return Number of bytes copied from \a ptr.
 * Test: unit test (test-oauth2.c: receive_curl_response).
 */
size_t
receive_curl_response( const char *ptr, size_t size, size_t nmemb,
					   void *state_data )
{
    struct curl_write_buffer_t *write_buffer = state_data;
    gsize                      to_copy       = size * nmemb;
    gchar                      *dest_buffer;
    int                        i;
    gsize                      to_allocate;

	/* Expand the buffer to receive the data. */
	to_allocate = write_buffer->size + to_copy + sizeof( gchar );
	write_buffer->data = g_realloc( write_buffer->data, to_allocate );
	dest_buffer = &( (gchar*) write_buffer->data )[ write_buffer->size ];

    /* Receive the data and zero-terminate the buffer so far. */
    for( i = 0; i < to_copy; i++ )
    {
		*dest_buffer++ = *ptr++;
    }
    *dest_buffer = '\0';
    write_buffer->size = write_buffer->size + to_copy;

	/* Return the number of bytes received. */
    return( to_copy );
}



/**
 * Create an input field and add it to a form.
 * @param form [in/out] The form that should include the input.
 * @param name The name of the input element.
 * @param value The value of the input element.
 * @return Nothing.
 */
void
add_input_to_form( form_field_t *form, const gchar *name, const gchar *value )
{
	input_field_t *input;

	input = g_new( input_field_t, 1 );
	input->name  = g_strdup( name );
	input->value = g_strdup( value );
	form->input_fields = g_slist_append( form->input_fields, input );
}



/**
 * Auxiliary function that adds an input element name and value to a CURL
 * post form.
 * @param input_field_ptr [in] Pointer to an input_field_t structure with
 *        input name and value.
 * @param form_vars_ptr [in/out] Pointer to a curl_form_vars_t structure with
 *        form parameters.
 * @return Nothing.
 */
void
fill_form_with_input( gpointer input_field_ptr, gpointer form_vars_ptr )
{
	input_field_t        *input_field = input_field_ptr;
	struct curl_httppost **form_post  = form_vars_ptr;
	const gchar *name;
	const gchar *value;

	name  = input_field->name;
	value = input_field->value;
	curl_formadd( &form_post[ 0 ], &form_post[ 1 ],
				  CURLFORM_COPYNAME, name,
				  CURLFORM_COPYCONTENTS, value,
				  CURLFORM_END );
}



/**
 * Submit a form according to its \a action attribute and return the response
 * from the host.
 * @param curl [in/out] CURL handle.
 * @param form [in] Form that should be submitted.
 * @return Raw HTML response, or \a NULL if the form could not be submitted.
 * Test: smoke test (test-oauth2.c: post_form) and implied (by testing the
 *       \a login_to_google function outside of the test suite).
 */
gchar*
post_form( CURL *curl, const form_field_t *form )
{
	const gchar                *form_name;
	const gchar                *form_value;
	const gchar                *form_action;

	struct curl_slist          *curl_headers   = NULL;
	struct curl_httppost       *form_post[ 2 ] = { NULL, NULL };
	struct curl_write_buffer_t html_response   = { .data = NULL, .size = 0 };

	form_name   = form->name;
	form_value  = form->value;
	form_action = form->action;

	/*  Post the form using receive_curl_response to store the reply. */
    /*
	curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
	*/
	if( global_config.ipv4_only == TRUE )
	{
		curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
	}

	/* Prepare for posting a form. */
	curl_easy_setopt( curl, CURLOPT_POST, 1 );

	/* Copy the form. */
	if( form_name != NULL )
	{
		curl_formadd( &form_post[ 0 ], &form_post[ 1 ],
					  CURLFORM_COPYNAME, form_name,
					  CURLFORM_COPYCONTENTS, form_value,
					  CURLFORM_END );
	}
	/* Copy the form's input elements. */
	g_slist_foreach( form->input_fields, fill_form_with_input, form_post );
	/* Enter the form in CURL. */
	curl_easy_setopt( curl, CURLOPT_HTTPPOST, form_post[ 0 ] );
	/* We may be redirected several times. */
	curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( curl, CURLOPT_UNRESTRICTED_AUTH, 1 );
	/* Set the URL. */
	curl_easy_setopt( curl, CURLOPT_URL, form_action );
	/* "Expect: 100-continue" is unwanted. */
	/*curl_headers = curl_slist_append( NULL, "Expect:" );*/
	curl_easy_setopt( curl, CURLOPT_HTTPHEADER, curl_headers );
	/* Send the request, receiving the response in html_response. */
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_response );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
	curl_easy_perform( curl );

	curl_slist_free_all( curl_headers );
	curl_formfree( form_post[ 0 ] );
	curl_easy_reset( curl );

	return( html_response.data );
}



/**
 * Auxiliary function used by \a destroy_form_inputs to free the memory
 * allocated by an input element.  Both the input element's strings and the
 * input element itself are destroyed.
 * @param input_ptr [out] Pointer to input whose memory should be deallocated.
 * @param user_data [in] Ignored.
 * @return Nothing.
 */
STATIC void
destroy_input_field( gpointer input_ptr, gpointer user_data )
{
	input_field_t *input = input_ptr;

	g_free( input->name );
	g_free( input->value );
	g_free( input );
}



/**
 * Free the memory allocated by a form's input elements.
 * @param form [out] The form whose input elements should be deallocated.
 * @return Nothing.
 */
void
destroy_form_inputs( form_field_t *form )
{
	g_slist_foreach( form->input_fields, destroy_input_field, NULL );
}



/**
 * Free the memory allocated by a form.
 * @param form [out] The form that is to be destroyed.
 * @return Nothing.
 */
void
destroy_form( form_field_t *form )
{
	destroy_form_inputs( form );
	g_free( form );
}



/**
 * Auxiliary function for \a decode_json_reply which invokes the custom
 * JSON decoder function.
 * @param node [in] Unused.
 * @param member_name [in] The name of the JSON node.
 * @param member_node [in] JSON node with data.
 * @param json_wrapper_ptr [in/out] Pointer to a json_wrapper_t structure
 *        that contains the custom JSON decoder function and the user data.
 * @return Nothing.
 */
STATIC void
decode_json_foreach_wrapper( JsonObject *node, const gchar *member_name,
							 JsonNode *member_node, gpointer json_wrapper_ptr )
{
	struct json_wrapper_t *json_wrapper = json_wrapper_ptr;

	/* Ignore the node, and pass only those json entries that hold a value. */
	if( JSON_NODE_HOLDS_VALUE( member_node ) )
	{
		json_wrapper->function( member_name, member_node, json_wrapper->data );
	}
}



/**
 * Decode a JSON response using a custom decoder function that parses the
 * the JSON-encoded values one at a time.
 * @param json_doc [in] JSON-encoded document.
 * @param json_decoder [in] Custom decoder function.
 * @param user_data [in/out] User data to pass to the custom decoder function.
 * @return Nothing.
 */
void
decode_json_reply( const gchar *json_doc,
				   void (*json_decoder)( const gchar *member_name,
										 JsonNode    *member_node,
										 gpointer    user_data ),
				   gpointer user_data )
{
	JsonParser         *json_parser;
	JsonNode           *node;
	JsonObject         *root;
	struct json_wrapper_t json_wrapper;

	/* Walk through the JSON response, beginning at the root. */
	json_parser = json_parser_new( );
	json_parser_load_from_data( json_parser, json_doc,
								strlen( json_doc ), NULL );
	node = json_parser_get_root( json_parser );
	root = json_node_get_object( node );
	json_wrapper.function = json_decoder;
	json_wrapper.data     = user_data;
	json_object_foreach_member( root, decode_json_foreach_wrapper,
								&json_wrapper );
	/* Release parser memory. */
	g_object_unref( json_parser );
}

