/**
 * \file postform.h
 * \brief Definitions for posting HTTP forms and receiving JSON replies.
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

#ifndef __GTASKS_POSTFORM_H
#define __GTASKS_POSTFORM_H

#include <config.h>
#include <glib.h>
#include <json-glib/json-glib.h>
#include <curl/curl.h>


#ifndef STRINGIFY
#define STRINGIFY( x ) XSTRINGIFY( x )
#define XSTRINGIFY( x ) #x
#endif

#define SET_JSON_MEMBER( structure, name )                       \
	if( g_strcmp0( member_name, STRINGIFY( name ) ) == 0 )       \
	{                                                            \
		structure->name = json_node_dup_string( member_node );   \
	}


typedef void (*json_decoder_function) ( const gchar *name,
										JsonNode    *node,
										gpointer    data );



/* Container for a generic callback function and its associated user data
   which is used by the \a decode_json_foreach_wrapper function to decode
   a JSON response. */
struct json_wrapper_t
{
	json_decoder_function function;
	gpointer              data;
};



/* The input_field_t structure contains the name and value attribute contents
   of an HTML input tag. */
typedef struct
{
	gchar *name;
	gchar *value;
} input_field_t;


/* The form_field_t structure contains a HTML form's name, value, and action,
   and a list of input fields. */
typedef struct
{
	gchar  *name;
	gchar  *value;
	gchar  *action;
	GSList *input_fields;
} form_field_t;


/* Define a custom data type that is passed by the CURL call-back for
   writing data received from Google. */
struct curl_write_buffer_t
{
	gchar *data;
	gsize size;
};


/*
 * Callback function for a CURL write.  The function maintains a dynamically
 * expanding buffer that may be filled successively by multiple callbacks.
 */
size_t receive_curl_response( const char *ptr, size_t size, size_t nmemb,
							  void *state_data );

/**
 * Create an input field and add it to a form.
 */
void add_input_to_form( form_field_t *form, const gchar *name,
						const gchar *value );

/*
 * Auxiliary function that adds an input element name and value to a CURL
 * post form.
 */
void fill_form_with_input( gpointer input_field_ptr, gpointer form_vars_ptr );

/*
 * Submit a form according to its \a action attribute and return the response
 * from the host.
 */
gchar *post_form( CURL *curl, const form_field_t *form,
				  struct curl_slist *headers );

/*
 * Free the memory allocated by a form's input elements.
 */
void destroy_form_inputs( form_field_t *form );

/*
 * Free the memory allocated by a form.
 */
void destroy_form( form_field_t *form );

/*
 * Decode a JSON response using a custom decoder function, which parses the JSON
 * document and places the output in a custom container.
 */
void decode_json_reply( const gchar *json_doc,
						json_decoder_function json_decoder,
						gpointer user_data );

/*
 * Auxiliary function for decode_json_reply which invokes the custom
 * JSON decoder function.
 */
void
decode_json_foreach_wrapper( JsonObject *node, const gchar *member_name,
							 JsonNode *member_node, gpointer json_wrapper_ptr );


#endif /* __GTASKS_POSTFORM_H */
