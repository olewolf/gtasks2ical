/**
 * \file gtasks.c
 * \brief Read and write Google Tasks.
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
#include <string.h>
#include <glib/gprintf.h>
#include "gtasks2ical.h"
#include "postform.h"
#include "gtasks.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


#define GOOGLE_TASKS_API "https://www.googleapis.com/tasks/v1/"



/* Global configuration data. */
extern struct configuration_t global_config;



/**
 * Submit a request to the Google Tasks API, optionally with body data.
 * NOTE:  body data is currently ignored.
 * @param curl [in] CURL handle.
 * @param method [in] HTTP method (POST, GET, etc...).
 * @param rest_uri [in] API URI (e.g., "users/@me/lists").
 * @param access_token [in] The applications authorizaton token.
 * @param body [in] Body message to include in the request, or \a NULL if no
 *        body content should be submitted.
 * @param curl_headers [in] Any CURL headers that may need to be submitted.
 * @return JSON response from the Google Tasks API.
 */
STATIC gchar*
send_gtasks_data( CURL *curl, const gchar *method, const gchar *rest_uri,
				  const gchar *access_token,
				  const gchar *body, struct curl_slist *curl_headers )
{
	gchar                      *authorization;
	gchar                      *url;
	struct curl_write_buffer_t html_response = { .data = NULL, .size = 0 };


	authorization = g_strconcat( "Authorization: Bearer ",
								 access_token, NULL );
	curl_headers = curl_slist_append( curl_headers,
									  "Accept: application/json" );
	curl_headers = curl_slist_append( curl_headers, authorization );

	curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
	if( global_config.ipv4_only == TRUE )
	{
		curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
	}
	curl_easy_setopt( curl, CURLOPT_CUSTOMREQUEST, method );
	curl_easy_setopt( curl, CURLOPT_HTTPHEADER, curl_headers );
	/* Assume redirections. */
	curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( curl, CURLOPT_UNRESTRICTED_AUTH, 1 );
	/* Set the URL. */
	url = g_strconcat( GOOGLE_TASKS_API, rest_uri, NULL );
	curl_easy_setopt( curl, CURLOPT_URL, url );
	/* Send the request, receiving the response in html_response. */
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_response );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
	curl_easy_perform( curl );

	g_free( url );
	g_free( authorization );
	curl_slist_free_all( curl_headers );
	curl_easy_reset( curl );

	return( html_response.data );
}



/**
 * Callback function that copies the attributes of a task list into a "lists"
 * container.
 * @param member_name [in] Name of the attribute.
 * @param member_node [in] Node that contains the attribute.
 * @param data_ptr [out] Pointer to a list element.
 * @return Nothing.
*/
STATIC void
copy_list_name_values( const gchar *member_name, JsonNode *member_node,
					   gpointer data_ptr )
{
	gtask_list_t *list = data_ptr;
	const gchar  *date_string;

	SET_JSON_MEMBER( list, id );
	SET_JSON_MEMBER( list, title );
	/* Create a timestamp version of the "updated" value. */
	if( g_strcmp0( member_name, "updated" ) == 0 )
	{
		date_string = json_node_get_string( member_node );
		g_time_val_from_iso8601( date_string, &list->updated );
	}
}



/**
 * Callback function that walks through the elements of a list member.
 * @param items [in] Not used.
 * @param index [in] Not used.
 * @param node [in] Node containing the array.
 * @param data_ptr [in] Pointer to the "lists" list that should be populated.
 * @return Nothing.
 */
STATIC void
copy_list_name( JsonArray *items, guint index, JsonNode *node,
				gpointer data_ptr )
{
	GSList                **lists = data_ptr;
	gtask_list_t          *list_entry;
	JsonObject            *root;
	struct json_wrapper_t json_wrapper;

	list_entry = g_new0( gtask_list_t, 1 );
	/* Copy the list name attributes into the list entry. */
	root = json_node_get_object( node );
	json_wrapper.function = copy_list_name_values;
	json_wrapper.data     = list_entry;
	json_object_foreach_member( root, decode_json_foreach_wrapper,
								&json_wrapper );
	/* Add the list name. */
	*lists = g_slist_append( *lists, list_entry );
}



/**
 * Callback function that extracts the array from a task lists JSON reply.
 * @param name [in] Name of the current node.
 * @param node [in] Node containing the top-level JSON reply members.
 * @param data_ptr [out] Pointer to the "lists" list that should be populated.
 * @return Nothing.
 */
STATIC void
decode_tasklists_json( const gchar *name, JsonNode *node, gpointer data_ptr )
{
	GSList    **lists = data_ptr;
	JsonArray *items;

	if( g_strcmp0( name, "items" ) == 0 )
	{
		items = json_node_get_array( node );
		json_array_foreach_element( items, copy_list_name, lists );
	}
}



void
debug_show_list( gpointer data, gpointer user_data )
{
	gtask_list_t *list = data;
	g_printf( "LIST: %s\n", list->title );
	g_printf( "  id = %s\n", list->id );
	g_printf( "  updated = %s\n", g_time_val_to_iso8601( &list->updated ) );
}



/**
 * Read the user's task lists.
 * @param curl [in] CURL handle.
 * @param access_token [in] Access token for the user's Google data.
 * @return List of task list names.
 */
GSList*
get_gtasks_lists( CURL *curl, const gchar *access_token )
{
	GSList            *lists = NULL;
	struct curl_slist *curl_headers = NULL;
	gchar             *json_response;

	json_response = send_gtasks_data( curl, "GET", "users/@me/lists",
									  access_token, NULL, NULL );
	decode_json_reply( json_response, decode_tasklists_json, &lists );
/*
	g_slist_foreach( lists, debug_show_list, NULL );
*/

	return( lists );
}



/**
 * Get information about a specified task list.
 * @param curl [in] CURL handle.
 * @param access_token [in] Access token for the user's Google data.
 * @param task_list_id [in] ID of the task list.
 * @return Information about the task list.
 */
gtask_list_t*
get_specified_gtasks_list( CURL *curl, const gchar *access_token,
						   const char *task_list_id )
{
	gchar             *uri;
	struct curl_slist *curl_headers = NULL;
	gchar             *json_response;
	gtask_list_t      *list_entry;

	/* Request information about the specified list. */
	uri = g_strconcat( "users/@me/lists/", task_list_id, NULL );
	json_response = send_gtasks_data( curl, "GET", uri,
									  access_token, NULL, NULL );
	g_free( uri );
	/* Copy the list name attributes into the list entry. */
	list_entry = g_new0( gtask_list_t, 1 );
	decode_json_reply( json_response, copy_list_name_values, list_entry );
/*
	debug_show_list( list_entry, NULL );
*/
	/* Return zero if an error occurred. */
	if( list_entry->title == NULL )
	{
		g_free( list_entry->id );
		g_free( list_entry );
		list_entry = NULL;
	}
	return( list_entry );
}


