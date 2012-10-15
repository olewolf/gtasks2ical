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
#include <libical/ical.h>
#include "gtasks2ical.h"
#include "postform.h"
#include "gtasks.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


#define GOOGLE_TASKS_API "https://www.googleapis.com/tasks/v1/"

struct tasks_page_t
{
	GSList *tasks;
	gchar  *next_page;
};


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

//	curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
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
	GTimeVal     updated_time;

	SET_JSON_MEMBER( list, id );
	SET_JSON_MEMBER( list, title );
	/* Create a timestamp version of the "updated" value. */
	if( g_strcmp0( member_name, "updated" ) == 0 )
	{
		date_string = json_node_get_string( member_node );
		g_time_val_from_iso8601( date_string, &updated_time );
		list->updated = g_date_time_new_from_timeval_local( &updated_time );
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
	g_printf( "  updated = %s\n", g_date_time_format( list->updated, "%F %R:%S %Z" ) );
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



/**
 * Copy the link type, description, and target to a link entry.
 * @param member_name [in] Either "type," "description," or "link."
 * @param member_node [in] The attribute value for the above type.
 * @param data_ptr [in] Pointer to the destination where the attributes are
 *        copied to.
 * @return Nothing.
 */
STATIC void
copy_link_attributes( const gchar *member_name, JsonNode *member_node,
					  gpointer data_ptr )
{
	gtask_link_t *link = data_ptr;

	g_printf( "DEBUG: copy_link_attributes\n" );

	SET_JSON_MEMBER( link, type );
	SET_JSON_MEMBER( link, description );
	SET_JSON_MEMBER( link, link );
}



/**
 * Copy all the Google Tasks links to a linked list.
 * @param links [in] Array of links decoded from the Google Tasks JSON reply.
 * @param index [in] Unused.
 * @param node [in] The node with the link attributes.
 * @param data_ptr [in] Pointer the destination list where the links are
 *        inserted.
 * @return Nothing.
 */
STATIC void
copy_link( JsonArray *links, guint index, JsonNode *node, gpointer data_ptr )
{
	GSList                **link_list = data_ptr;
	JsonObject            *root;
	struct json_wrapper_t json_wrapper;
	gtask_link_t          *link;

	g_printf( "DEBUG: copy_link\n" );

	/* Copy the task attributes into the task. */
	link = g_new0( gtask_link_t, 1 );
	json_wrapper.function = copy_link_attributes;
	json_wrapper.data     = link;
	root = json_node_get_object( node );
	json_object_foreach_member( root, decode_json_foreach_wrapper,
								&json_wrapper );
	*link_list = g_slist_append( *link_list, link );
}



/**
 * Callback function that assigns values to the task attributes.
 * @param name [in] Name of the current node.
 * @param node [in] Current node.
 * @param page_ptr [out] Pointer to tasks structure.
 * @return Nothing.
 * Test: manual.
 */
STATIC void
copy_task_values( const gchar *member_name, JsonNode *member_node,
				  gpointer data_ptr )
{
	gtask_t     *task = data_ptr;
	const gchar *date_string;
	JsonArray   *links;
	GTimeVal    timeval;

	SET_JSON_MEMBER( task, id );
	SET_JSON_MEMBER( task, etag );
	SET_JSON_MEMBER( task, title );
	SET_JSON_MEMBER( task, parent );
	SET_JSON_MEMBER( task, notes );
	SET_JSON_MEMBER( task, status );
	if( g_strcmp0( member_name, "updated" ) == 0 )
	{
		date_string = json_node_get_string( member_node );
		g_time_val_from_iso8601( date_string, &timeval );
		task->updated = g_date_time_new_from_timeval_local( &timeval );
	}
	else if( g_strcmp0( member_name, "selfLink" ) == 0 )
	{
		task->self_link = json_node_dup_string( member_node );
	}
	else if( g_strcmp0( member_name, "position" ) == 0 )
	{
		task->position = json_node_dup_string( member_node );
	}
	else if( g_strcmp0( member_name, "due" ) == 0 )
	{
		date_string = json_node_get_string( member_node );
		g_time_val_from_iso8601( date_string, &timeval );
		task->due = g_date_time_new_from_timeval_local( &timeval );
	}
	else if( g_strcmp0( member_name, "completed" ) == 0 )
	{
		date_string = json_node_get_string( member_node );
		g_time_val_from_iso8601( date_string, &timeval );
		task->completed = g_date_time_new_from_timeval_local( &timeval );
	}
	else if( g_strcmp0( member_name, "deleted" ) == 0 )
	{
		task->deleted = json_node_get_boolean( member_node );
	}
	else if( g_strcmp0( member_name, "hidden" ) == 0 )
	{
		task->hidden = json_node_get_boolean( member_node );
	}
	else if( g_strcmp0( member_name, "links" ) == 0 )
	{
		links = json_node_get_array( member_node );
		json_array_foreach_element( links, copy_link, &task->links );
	}
}



/**
 * Callback function that walks through an array of task attributes to compile
 * a task.
 * @param items [in] Not used.
 * @param index [in] Not used.
 * @param node [in] Node containing the array.
 * @param data_ptr [in] Pointer to the task container that should be populated.
 * @return Nothing.
 * Test: manual.
 */
STATIC void
copy_task( JsonArray *items, guint index, JsonNode *node, gpointer data_ptr )
{
	struct tasks_page_t   *tasks_page = data_ptr;
	JsonObject            *root;
	struct json_wrapper_t json_wrapper;
	gtask_t               *task;

	/* Copy the task attributes into the task. */
	task = g_new0( gtask_t, 1 );
	json_wrapper.function = copy_task_values;
	json_wrapper.data     = task;
	root = json_node_get_object( node );
	json_object_foreach_member( root, decode_json_foreach_wrapper,
								&json_wrapper );
	tasks_page->tasks = g_slist_append( tasks_page->tasks, task );
}



/**
 * Decode the "next page" token and the tasks in the current list.
 * @param name [in] Name of the current node.
 * @param node [in] Current node.
 * @param page_ptr [out] Pointer to page and tasks container.
 * @return Nothing.
 * Test: manual.
 */
STATIC void
decode_task_page( const gchar *name, JsonNode *node, gpointer page_ptr )
{
	struct tasks_page_t *tasks_page = page_ptr;
	JsonArray           *items;
	gtask_t             *task;

	/* Store the "next page" token. */
	if( g_strcmp0( name, "nextPageToken" ) == 0 )
	{
		tasks_page->next_page = json_node_dup_string( node );
	}
	/* The items array is an array of tasks. */
	else if( g_strcmp0( name, "items" ) == 0 )
	{
		items = json_node_get_array( node );
		json_array_foreach_element( items, copy_task, tasks_page );
	}
}




void
debug_show_gtimeval( GDateTime *t )
{
	g_printf( "%s", g_date_time_format( t, "%F %R:%S %Z" ) );
}

void
debug_show_task( gpointer task_ptr, gpointer data )
{
	gtask_t *task = task_ptr;
	g_printf( "ID: %s\n", task->id );
	g_printf( "Etag: %s\n", task->etag );
	g_printf( "Title: %s\n", task->title );
	g_printf( "Updated: " ); debug_show_gtimeval( task->updated );
	    g_printf( "\n" );
	g_printf( "Self_link: %s\n", task->self_link );
	g_printf( "Parent: %s\n", task->parent );
	g_printf( "Position: %s\n", task->position );
	g_printf( "Notes: %s\n", task->notes );
	g_printf( "Status: %s\n", task->status );
	g_printf( "Due: " ); debug_show_gtimeval( task->due );
	    g_printf( "\n" );
	g_printf( "Completed: " ); debug_show_gtimeval( task->completed );
	    g_printf( "\n" );
	g_printf( "Deleted: %d\n", task->deleted );
	g_printf( "Hidden: %d\n", task->hidden );
}

void
debug_show_tasks( GSList *tasks )
{
	g_slist_foreach( tasks, debug_show_task, NULL );
}



/**
 * Read all tasks for a particular task list.
 * @param curl [in] CURL handle.
 * @param access_token [in] Access token for the user's Google data.
 * @param task_list_id [in] ID of the task list.
 * @param page_token [in] Page token for requesting the next round of tasks.
 * @return Information about the task list.
 * Test: manual.
 */
GSList*
get_all_list_tasks( CURL *curl, const gchar *access_token,
					const gchar *task_list_id, const gchar *page_token )
{
	gchar               *uri;
	gchar               *page;
	struct curl_slist   *curl_headers = NULL;
	gchar               *json_response;
	struct tasks_page_t tasks_page = { NULL, NULL };
	GSList              *tasks;
	GSList              *tasks_to_append;

	/* Indicate which page is requested. */
	if( page_token != NULL )
	{
		page = g_strconcat( "?pageToken=", page_token, NULL );
	}
	else
	{
		page = NULL;
	}
	/* Specify the list in the URI. */
	uri = g_strconcat( "lists/", task_list_id, "/tasks", page, NULL );
	g_free( page );
	/* Request the tasks. */
	json_response = send_gtasks_data( curl, "GET", uri,
									  access_token, NULL, NULL );
	g_free( uri );

	/* Decode the items list and the next-page token. */
	g_printf( "json_response = %s\n", json_response );
	decode_json_reply( json_response, decode_task_page, &tasks_page );
	g_free( json_response );
	tasks = tasks_page.tasks;

	/* If there are more pages, request the next page and append its task
	   list to our growing grande list o' tasks. */
	if( tasks_page.next_page != NULL )
	{
		tasks_to_append = get_all_list_tasks( curl, access_token, task_list_id,
											  tasks_page.next_page );
		g_free( tasks_page.next_page );
		tasks = g_slist_concat( tasks, tasks_to_append );
	}

//	debug_show_tasks( tasks );

	return( tasks );
}





/*
gchar*
convert_gtask_to_vcalendar( const gtask_t *gtask )
{

	icalcomponent *todo;
	icalproperty  *property;
	icalparameter *param;
	struct icaltimetype atime;

	todo = icalcomponent_vanew(
		ICAL_VCALENDAR_COMPONENT,
		icalproperty_new_version( "2.0" ),
		icalproperty_new_prodid( "-//Google//gtasks2ical//EN" ),

		icalcomponent_vanew(
			ICAL_VTODO_COMPONENT,
			icalproperty_new_dtstamp( atime ),
			icalproperty_new_uid( "guid1.host1.com" ),
			icalproperty_vanew_organizer(
				"youremail@gmail.com",
				icalparameter_new_role( ICAL_ROLE_CHAIR ),
				0 ),
			icalproperty_vanew_attendee(
				"anotheremail@gmail.com",
				icalparameter_new_role( ICAL_ROLE_REQPARTICIPANT ),
				icalparameter_new_rsvp( 1 ),
				icalparameter_new_cutype( ICAL_CUTYPE_GROUP ),
				0 ),
			icalproperty_new_location( "Office 7" ),
			0 ),
		0 )
		);
}
*/



/**
 * Read a specific task from a tasks list.
 * @param curl [in] CURL handle.
 * @param access_token [in] Access token for the user's Google data.
 * @param task_list_id [in] ID of the task list.
 * @param task_id [in] The ID of the task to read.
 * @return The requested task.
 * Test: manual.
 */
gtask_t*
get_specified_task( CURL *curl, const gchar *access_token,
					const gchar *task_list_id, const gchar *task_id )
{
	gchar             *uri;
	struct curl_slist *curl_headers = NULL;
	gchar             *json_response;
	gtask_t           *task;

	/* Specify the list and the task in the URI. */
	uri = g_strconcat( "lists/", task_list_id, "/tasks/", task_id, NULL );
	/* Request the tasks. */
	json_response = send_gtasks_data( curl, "GET", uri,
									  access_token, NULL, NULL );
	g_printf( "json_response = %s\n", json_response );
	g_free( uri );

	/* Decode the items list and the next-page token. */
	task = g_new0( gtask_t, 1 );
	decode_json_reply( json_response, copy_task_values, task );
	g_free( json_response );

//	debug_show_task( task, NULL );

	return( task );
}
