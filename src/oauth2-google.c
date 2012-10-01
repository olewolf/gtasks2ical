/**
 * \file oauth2-google.c
 * \brief Authenticate with Google without bothering the user.
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
#include "oauth2-google.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"

#define STRINGIFY( x ) XSTRINGIFY( x )
#define XSTRINGIFY( x ) #x

#define SET_JSON_MEMBER( structure, name )                       \
	if( g_strcmp0( member_name, STRINGIFY( name ) ) == 0 )       \
	{                                                            \
		structure->name = json_node_dup_string( member_node );   \
	}


struct json_wrapper_t
{
	void     (*function)( const gchar *member_name,
						  JsonNode    *member_node,
						  gpointer    user_data );
	gpointer data;
};


extern struct configuration_t global_config;


/**
 * Retrieve the names and values of all the input elements contained in an
 * HTML form node.
 * @param parent_node [in] Parent XML node (e.g., a form node).
 * @return List of input fields.
 * Test: implied (test-oauth2.c: get_forms).
 */
STATIC GSList*
get_input_fields( const xmlNode *parent_node )
{
	const xmlNode *node;
	GSList        *input_fields = NULL;
	input_field_t *new_input_field;
	xmlChar       *name;
	xmlChar       *value;
	GSList        *children_input_fields;

	for( node = parent_node->xmlChildrenNode; node; node = node->next )
	{
		if( node->type == XML_ELEMENT_NODE )
		{
			/* If the node is an input then get its name and value. */
			if( xmlStrEqual( node->name, (xmlChar*) "input" ) )
			{
				/* Read the input element's attributes. */
				name  = xmlGetProp( (xmlNodePtr) node, (xmlChar*) "name" );
				value = xmlGetProp( (xmlNodePtr) node, (xmlChar*) "value" );

				/* Insert the input field into the input fields list. */
				new_input_field = g_new( input_field_t, 1 );
				new_input_field->name  = g_strdup( (gchar*) name );
				new_input_field->value = g_strdup( (gchar*) value );
				input_fields = g_slist_append( input_fields, new_input_field );
				xmlFree( name );
				xmlFree( value );
			}
			else
			{
				/* For any other node name, investigate its child nodes. */
				children_input_fields = get_input_fields( node );
				input_fields = g_slist_concat( input_fields,
											   children_input_fields );
			}
		}
	}

	return( input_fields );
}



/**
 * Recursive function that builds a list of forms in an XML document
 * including their input elements.
 * @param node [in] Current depth of the XML document.
 * @param form_fields [in] Forms list to which the forms will be appended.
 * @return New value for the forms list.
 * Test: implied (test-oauth2.c: get_forms).
 */
STATIC GSList*
get_form_fields( const xmlNode *node, GSList *form_fields )
{
	const gchar   *node_contents;
	const xmlNode *current_node;

	xmlChar      *form_action;
	xmlChar      *form_name;
	xmlChar      *form_value;
	GSList       *input_fields;
	form_field_t *new_form_field;

	/* Search the XML tree. */
	for( current_node = node; current_node; current_node = current_node->next )
	{
		if( current_node->type == XML_ELEMENT_NODE )
		{
			/* If a form is found then get the form's attributes and
			   input children. */
			if( xmlStrEqual( current_node->name, (xmlChar*) "form" ) )
			{
				/* Read form attributes. */
				form_action = xmlGetProp( (xmlNodePtr) current_node,
										  (xmlChar*) "action" );
				form_name   = xmlGetProp( (xmlNodePtr) current_node,
										  (xmlChar*) "name" );
				form_value  = xmlGetProp( (xmlNodePtr) current_node,
										  (xmlChar*) "value" );
				/* Get the form's input children. */
				input_fields = get_input_fields( current_node );

				/* Insert the form field into the forms list. */
				new_form_field = g_new( form_field_t, 1 );
				new_form_field->action       = g_strdup( (gchar*) form_action );
				new_form_field->name         = g_strdup( (gchar*) form_name );
				new_form_field->value        = g_strdup( (gchar*) form_value );
				new_form_field->input_fields = input_fields;
				form_fields = g_slist_append( form_fields, new_form_field );
				xmlFree( form_action );
				xmlFree( form_name );
				xmlFree( form_value );
			}
			/* Otherwise, if a form was not found, descend to the next level
			   of the xml structure. */
			else
			{
				form_fields = get_form_fields( current_node->xmlChildrenNode,
											   form_fields );
			}
		}
	}

	return( form_fields );
}



/**
 * Retrieve a list of all the forms and their input fields in the HTML
 * document.  The document is expected to be somewhat malformed, and is
 * cleaned up via libtidy before parsing it using libxml2.
 * @param raw_html_page [in] HTML document with forms.
 * @return List of forms.
 * Test: unit test (test-oauth2.c: get_forms).
 */
STATIC GSList*
get_forms( const gchar *raw_html_page )
{
	TidyBuffer tidy_output;
	TidyBuffer tidy_error;
	TidyDoc    tidy_doc;
	int        tidy_status;

	xmlDocPtr html_doc;
	size_t    html_doc_size;
	xmlNode   *node;
	GSList    *form_fields = NULL;

	/* Google's page isn't well-formed HTML, but by churning it through
	   libtidy and then removing CDATA it can be parsed.  Perform some other
	   clean-ups, too, like removing blank nodes and such. */
	tidy_doc = tidyCreate( );
	tidyBufInit( &tidy_output );
	tidyBufInit( &tidy_error );
	/* Convert to XHTML, strip smart quotes, and convert entities to
	   numeric values. */
	tidyOptSetBool( tidy_doc, TidyXhtmlOut, yes );
	tidyOptSetBool( tidy_doc, TidyMakeBare, yes );
	tidyOptSetBool( tidy_doc, TidyNumEntities, yes );
	tidyOptSetBool( tidy_doc, TidyQuoteAmpersand, yes );
	tidyOptSetBool( tidy_doc, TidyHideComments, yes );
	/* Clean-up the document. */
	tidy_status = tidySetErrorBuffer( tidy_doc, &tidy_error );
	/* Parse the input. */
	if( 0 <= tidy_status )
	{
		tidy_status = tidyParseString( tidy_doc, raw_html_page );
	}
	/* Tidy up the input. */
	if( 0 <= tidy_status )
	{
		tidy_status = tidyCleanAndRepair( tidy_doc );
	}
	if( 0 <= tidy_status )
	{
		tidy_status = tidyRunDiagnostics( tidy_doc );
	}
	/* Force output if parsing errors were encountered. */
	if( 1 < tidy_status )
	{
		if( tidyOptSetBool( tidy_doc, TidyForceOutput, yes ) == 0 )
		{
			tidy_status = -1;
		}
	}
	/* Create the tidied output. */
	if( 0 <= tidy_status )
	{
		tidy_status = tidySaveBuffer( tidy_doc, &tidy_output );
	}
	if( tidy_status < 0 )
	{
		g_printf( "ERROR: cannot parse the HTML document\n" );
		exit( EXIT_FAILURE );
	}

	/* Now parse the document with some wiggle room for errors. */
	html_doc_size = strlen( (const char*) tidy_output.bp );
    html_doc = xmlReadMemory( (const char*) tidy_output.bp, html_doc_size,
							  "tidy.html", NULL,
							  XML_PARSE_RECOVER | XML_PARSE_NOENT |
							  XML_PARSE_NOERROR | XML_PARSE_NOWARNING |
							  XML_PARSE_NOBLANKS | XML_PARSE_NONET |
							  XML_PARSE_NSCLEAN | XML_PARSE_NOCDATA );
	tidyBufFree( &tidy_error );
	tidyBufFree( &tidy_output );
	tidyRelease( tidy_doc );

	/* Retrieve all the form elements. */
	if( html_doc != NULL )
	{
		node = xmlDocGetRootElement( html_doc );
		if( node != NULL )
		{
			form_fields = get_form_fields( node, NULL );
		}
		xmlFreeDoc( html_doc );
	}

	return( form_fields );
}



/**
 * Auxiliary function for \a search_form_by_action_and_inputs that determines
 * whether the name of an input_field_t matches one of those in a list of
 * names.
 * @param input_ptr [in] Pointer to the input field.
 * @param input_names_ptr [in] Pointer to a list of names.
 * @return \a 0 if the name matches, or \a 1 otherwise.
 * Test: unit test (test-oauth2.c: search_input_by_name).
 */
STATIC int
search_input_by_name( gconstpointer input_ptr, gconstpointer input_names_ptr )
{
	const input_field_t *input        = input_ptr;
	const gchar *const  *input_names = input_names_ptr;
	gint                idx           = 0;
	gint                name_found    = 1;

	/* If the names criteria list is empty, a name search is always
	   successful. */
	if( input_names[ idx ] == NULL )
	{
		name_found = 0;
	}
	/* Find out if the input element is named according to the list of input
	   names. */
	else
	{
		while( input_names[ idx ] )
		{
			/* Set the "found" flag if the name matches. */
			if( ( input->name != NULL ) && 
				( g_strcmp0( input->name, input_names[ idx ] ) == 0 ) )
			{
				name_found = 0;
				break;
			}
			idx++;
		}
	}

	return( name_found );
}



/**
 * Auxiliary function for \a find_form that searches a form list for a form
 * with the specified action and input element names.
 * @param form_ptr [in] Pointer to a form.
 * @param search_ptr [in] Pointer to a form_search_t structure with the form's
 *        action and input element names.
 * @return \a 0 if the form matches, or \a 1 otherwise.
 * Test: unit test (test-oauth2.c: search_form_by_action_and_inputs).
 */
STATIC int
search_form_by_action_and_inputs( gconstpointer form_ptr,
								  gconstpointer search_ptr )
{
	const form_field_t         *form      = form_ptr;
	const struct form_search_t *search    = search_ptr;
	gboolean                   form_name_match = FALSE;
	GSList                     *input_fields;
	GSList                     *input;
	gint                       form_found = 1;
	size_t                     max_action_chars;

	/* Compare form names. Ignore the form name if it is not specified
	   as a search criterion. */
	if( search->form_action != NULL )
	{
		/* The form name is a search criterion, so check if it matches. */
		if( form->action != NULL )
		{
			max_action_chars = strlen( search->form_action );
			if( strncmp( form->action, search->form_action,
				 max_action_chars  ) == 0 )
			{
				form_name_match = TRUE;
			}
		}
	}
	/* If the form name doesn't matter, the form name will always be
	   found. */
	else
	{
		form_name_match = TRUE;
	}
	
	/* If the form action was found, determine whether the input fields
	   match, too. */
	if( form_name_match )
	{
		input_fields = form->input_fields;
		input = g_slist_find_custom( input_fields,
									 (gconstpointer) search->input_names,
									 search_input_by_name );
		if( input != NULL )
		{
			form_found = 0;
		}
	}

	return( form_found );
}



/**
 * Find a form element on an HTML page based on the form action and the names
 * of some of the input elements.
 * @param html_page [in] Raw HTML page that may contain the desired form.
 * @param form_action [in] The action associated with the form.
 * @param input_names [in] List of names of input elements in the form.
 * @return Identified form, or \a NULL if no form was found.
 * Test: unit test (test-oauth2.c: find_form).
 */
STATIC form_field_t*
find_form( const gchar *raw_html_page, const gchar *form_action,
		   const gchar *const *input_names )
{
	GSList               *form_list;
	struct form_search_t form_action_inputs;
	GSList               *form_found;
	form_field_t         *form;

	/* Read all the forms from the HTML page. */
	form_list = get_forms( raw_html_page );
	/* Search the forms for the action name. */
	form_action_inputs.form_action = form_action;
	form_action_inputs.input_names = input_names;
	form_found = g_slist_find_custom( form_list, &form_action_inputs,
									  search_form_by_action_and_inputs );
	if( form_found != NULL )
	{
		form = form_found->data;
	}
	else
	{
		form = NULL;
	}
	return( form );
}



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
STATIC size_t
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
 * Retrieve the HTML body of the page at the specified URL.
 * @param curl [in/out] CURL handle.
 * @param url [in] URL of the HTML page.
 * @return Raw HTML body of the page, or \a NULL if the page couldn't be read.
 * Test: unit test (test-oauth2.c: read_url).
 */
STATIC gchar*
read_url( CURL *curl, const gchar *url )
{
	struct curl_write_buffer_t html_body = { NULL, 0 };
	CURLcode errcode;

	/* Receive the HTML body only, using receive_curl_response to store it
	   in html_body. */
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_body );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
	if( global_config.ipv4_only == TRUE )
	{
		curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
	}
	/* We'll probably be redirected. */
	curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( curl, CURLOPT_UNRESTRICTED_AUTH, 1 );
	/* Set the URL. */
	curl_easy_setopt( curl, CURLOPT_URL, url );
	/* Send the request, receiving the response in html_body. */
	errcode = curl_easy_perform( curl );
	if( errcode != CURLE_OK )
	{
		g_fprintf( stderr, "Could not receive web page" );
//		exit( EXIT_FAILURE );
	}
	curl_easy_reset( curl );
	return( html_body.data );
}



/**
 * Read a HTML page from a specified URL, and locate a form on the page
 * based on the form action and some of the names of its input elements.
 * @param curl [in/out] CURL handle.
 * @param url [in] URL for the HTML page.
 * @param form_action [in] The form action.
 * @param input_names [in] List of names of input elements in the form.
 * @return Form data, or \a NULL if the form was not found.
 * Test: unit test (test-oauth2.c: get_form_from_url).
 */
STATIC form_field_t*
get_form_from_url( CURL* curl, const gchar *url, const gchar *form_action,
				   const gchar *const *input_names )
{
	gchar        *raw_html;
	form_field_t *form;

	/* Get the HTML page. */
	raw_html = read_url( curl, url );
	/* Get the form and its input elements. */
	form = find_form( raw_html, form_action, input_names );
	g_free( raw_html );

	return( form );
}



/**
 * Create an input field and add it to a form.
 * @param form [in/out] The form that should include the input.
 * @param name The name of the input element.
 * @param value The value of the input element.
 * @return Nothing.
 */
STATIC void
add_input_to_form( form_field_t *form, const gchar *name, const gchar *value )
{
	input_field_t *input;

	input = g_new( input_field_t, 1 );
	input->name  = g_strdup( name );
	input->value = g_strdup( value );
	form->input_fields = g_slist_append( form->input_fields, input );
}



/**
 * Auxiliary function for \a modify_form which replaces the data of an input
 * element with new data.  If the input element does not exist, the new
 * input element is added to the form.
 * @param input_ptr [in] Pointer to the input field that contains new data.
 * @param form_ptr [out] Pointer to the form whose input elements should be
 *        modified.
 * @return Nothing.
 * Test: unit test (test-oauth.c: modify_form_inputs).
 */
STATIC void
modify_form_inputs( gpointer input_ptr, gpointer form_ptr )
{
	const input_field_t *input_field = input_ptr;
	form_field_t        *form        = form_ptr;
	const gchar         *input_names[ 2 ] = { NULL, NULL };
	GSList              *search_entry;
	input_field_t       *input;
	input_field_t       *new_input;

	/* Locate the input in the form that should be modified. */
	input_names[ 0 ] = input_field->name;
	search_entry = g_slist_find_custom( form->input_fields,
									   (gconstpointer) input_names,
									   search_input_by_name );

	/* Modify the input's value. */
	if( search_entry != NULL )
	{
		input = search_entry->data;
		g_free( input->value );
		input->value = g_strdup( input_field->value );
	}
	/* Or add the input if it wasn't found. */
	else
	{
		add_input_to_form( form, input_field->name, input_field->value );
	}
}



/**
 * Update input elements in a form with new data, or add new input elements
 * in case they do not exist.
 * @param form [in/out] Form containing input elements that should be modified.
 * @param inputs_to_modify [in] List of input elements with new data.
 * @return Nothing.
 * Test: unit test (test-oauth2.c: modify_form).
 */
STATIC void
modify_form( form_field_t *form, const GSList *inputs_to_modify )
{
	/* Update the form's input elements. */
	g_slist_foreach( (GSList*)inputs_to_modify, modify_form_inputs, form );
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
STATIC void
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
STATIC gchar*
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
	curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
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
STATIC void
destroy_form_inputs( form_field_t *form )
{
	g_slist_foreach( form->input_fields, destroy_input_field, NULL );
}



/**
 * Free the memory allocated by a form.
 * @param form [out] The form that is to be destroyed.
 * @return Nothing.
 */
STATIC void
destroy_form( form_field_t *form )
{
	destroy_form_inputs( form );
	g_free( form );
}



/**
 * Login to Gmail.  This function attempts to pass the first part of the
 * login where the user is requested to enter his/her username and password.
 * @param username [in] The user's Google username (username@gmail.com).
 * @param password [in] The user's Google password.
 * @return \a TRUE if the login was successfull, or \a FALSE otherwise.
 * Test: manual (via non-automated test).
 */
gboolean
login_to_gmail( CURL *curl, const gchar *username, const gchar *password )
{
	static const gchar *login_url =
		"https://accounts.google.com/ServiceLogin?"
		"passive=true&rm=false&"
		"continue=https://mail.google.com/mail/";
	static const gchar *login_action =
		"https://accounts.google.com/ServiceLoginAuth";
	static const gchar *login_inputs[ ] = { "Email", "Passwd", NULL };
	form_field_t       *login_form;
	GSList             *inputs_to_modify;
	input_field_t      input_email    = { "Email", (gchar*)username };
	input_field_t      input_password = { "Passwd", (gchar*)password };
	gchar              *login_response;
	gint               response_length;
	gboolean           logged_in;

	/* Get the original login form. */
	login_form = get_form_from_url( curl, login_url,
									login_action, login_inputs );
	/* Modify login form. */
	inputs_to_modify = g_slist_append( NULL, &input_email );
	inputs_to_modify = g_slist_append( inputs_to_modify, &input_password );
	modify_form( login_form, inputs_to_modify );
	g_slist_free( inputs_to_modify );
	/* Submit the login form. */
	login_response = post_form( curl, login_form );
	destroy_form( login_form );

	#ifdef AUTOTEST
	if( login_response != NULL )
	{
		g_printf( "%s\n", login_response );
	}
	#endif

	/* Determine whether some typical login page content is present in the
	   response page. */
	response_length = strlen( login_response );
	if( g_strstr_len( login_response, response_length,
					  "https://www.google.com/settings/ads/preferences" ) &&
		g_strstr_len( login_response, response_length,
					 "href=\"https://plus.google.com/u/0/me\"" ) &&
		g_strstr_len( login_response, response_length,
					  "href=\"https://accounts.google.com/AddSession" ) &&
		g_strstr_len( login_response, response_length,
					  "\"GMAIL_CB\",GM_START_TIME" ) )
	{
		logged_in = TRUE;
	}
	else
	{
		logged_in = FALSE;
	}

	return( logged_in );
}



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



STATIC void
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



STATIC void
parse_tokens_json( const gchar *member_name,
				   JsonNode *member_node, gpointer access_ptr )
{
	access_code_t *access_code = access_ptr;
	time_t        expiration;

	SET_JSON_MEMBER( access_code, access_token );
	SET_JSON_MEMBER( access_code, refresh_token );
	/* Convert expiration time to a timestamp. */
	if( g_strcmp0( member_name, "expires_in" ) )
	{
		expiration = json_node_get_int( member_node );
		access_code->expiration_timestamp = time( NULL ) + expiration;
	}
}



/**
 * Obtain a device code for device access.  The CURL session must include the
 * user's login to Google, which may be obtained by calling \a login_to_gmail
 * with the user's credentials.
 * @param curl [in] CURL handle.
 * @param client_id [in] Google Developer Client ID.
 * @return Device code or \a NULL if an error occurred.
 * Test: manual (via non-automated test).
 */
struct user_code_t*
obtain_device_code( CURL *curl, const gchar *client_id )
{
	static const gchar *endpoint     =
		"https://accounts.google.com/o/oauth2/auth";
	static const gchar *redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
	static const gchar *scope        = "https://www.googleapis.com/auth/tasks";
	gchar              *request_url;

	form_field_t       *authorization_form;
	static const gchar *auth_form_action =
		"https://accounts.google.com/o/oauth2/approval";
	static const gchar *auth_input_names[ ] = { "submit_access", NULL };
	gchar              *auth_response;
	GRegex             *code_regex;
	GMatchInfo         *match_info;
	gchar              *code;
	struct user_code_t *user_code;

	/* Build the URL for requesting access to the user's data. */
	request_url = g_strconcat( endpoint, "?response_type=code&client_id=",
							   client_id, "&scope=", scope,
							   "&redirect_uri=", redirect_uri, NULL );
	/* Retrieve the data access authoriziation form. */
	authorization_form = get_form_from_url( curl, request_url, auth_form_action,
											auth_input_names );
	g_free( request_url );
	/* Submit the form to pretend the user has authorized the application. */
	if( authorization_form != NULL )
	{
		auth_response = post_form( curl, authorization_form );
		/* Grep the authorization code from the title. */
		code_regex = g_regex_new(
			"<title>\\s*[^=]+=\\s*([a-zA-Z0-9_/-]+)", 0, 0, NULL );
		if( g_regex_match( code_regex, auth_response, 0, &match_info ) )
		{
			user_code = g_new0( struct user_code_t, 1 );
			user_code->device_code = g_match_info_fetch( match_info, 1 );
		}
		else
		{
			user_code = NULL;
		}
		g_match_info_free( match_info );
		g_regex_unref( code_regex );
	}
	else
	{
		user_code = NULL;
	}

	return( user_code );
}



STATIC access_code_t*
obtain_access_code( CURL *curl, const gchar *device_code,
					const gchar *client_id, const gchar *client_secret )
{
	form_field_t request_form =
	{
		.name = NULL,
		.value = NULL,
		.action = "https://accounts.google.com/o/oauth2/token",
		.input_fields = NULL
	};
	gchar         *token_response;
	access_code_t *access_code;

	/* Build an authorization code request form. */
	add_input_to_form( &request_form, "code", device_code );
	add_input_to_form( &request_form, "client_id", client_id );
	add_input_to_form( &request_form, "client_secret", client_secret );
	add_input_to_form( &request_form, "grant_type",
					   "http://oauth.net/grant_type/device/1.0" );
	/* Request the authorization code. */
	token_response = post_form( curl, &request_form );
	destroy_form_inputs( &request_form );
	/* Decode the JSON response. */
	access_code = g_new0( access_code_t, 1 );
	decode_json_reply( token_response, parse_tokens_json, access_code );
	g_free( token_response );

	if( ( access_code->access_token == NULL ) ||
		( access_code->refresh_token == NULL ) ||
		( access_code->expiration_timestamp == (time_t) 0 ) )
	{
		g_free( access_code->access_token );
		g_free( access_code->refresh_token );
		g_free( access_code );
		access_code = NULL;
	}

	return( access_code );
}



STATIC gboolean
submit_approval_form( CURL *curl, const gchar *approval_page )
{
	form_field_t       *approval_form;
	const gchar *const approval_names[ ] = { "submit_access", NULL };
	gchar              *form_response;
	gboolean           success;

	approval_form = find_form( approval_page,
						"https://accounts.google.com/o/oauth2/",
					    approval_names );
	if( approval_form != NULL )
	{
		g_printf( "Submitting approval form\n" );

		/* Submit the approval form. */
		form_response = post_form( curl, approval_form );

		destroy_form( approval_form );
		g_free( form_response );
		success = TRUE;
	}
	else
	{
		success = FALSE;
	}

	return( success );
}



/**
 *
 */
void
authorize_application( CURL *curl )
{
	struct user_code_t *user_code;
	form_field_t       *user_code_form;
	const gchar *const user_code_names[ ] = { NULL };
	input_field_t      enter_code = { .name = "user_code", .value = NULL };
	GSList             *input_list;
	gchar              *form_response;
	access_code_t      *access_code;
	gboolean           success;

	/* Obtain a device code, a user code, and the verification URL. */
	user_code = obtain_device_code( curl, global_config.client_id );
	if( user_code != NULL )
	{
		g_printf( "User code obtained.\n" );
		/* As of October 1, 2012, there is a bug in Google's authentication
		   API that prevents devices from accessing the Tasks scope.  To
		   work around this, determine whether an access token is required. */

		/* No access token is required; the application is already
		   authorized. */
		if( user_code->verification_url == NULL )
		{
			
		}
		/* An access code is required. */
		else
		{
            /* Submit the user code at the verification URL. */
			user_code_form = get_form_from_url( curl,
												user_code->verification_url,
							"https://accounts.google.com/o/oauth2/device/auth",
							user_code_names );
			if( user_code_form != NULL )
			{
				enter_code.value = (gchar*) user_code->user_code;
				input_list = g_slist_append( NULL, &enter_code );
				modify_form( user_code_form, input_list );
				g_slist_free( input_list );
				form_response = post_form( curl, user_code_form );
				destroy_form( user_code_form );

				/* The response from the verification URL is page where the
				   user is requested to authorize the application to access the
				   user's data.  Find the approval form and submit it. */
				g_printf( "Locating approval form\n" );
				success = submit_approval_form( curl, form_response );
				g_free( form_response );
				if( success == TRUE )
				{
					/* The device is now approved and may request an access
					   token and a refresh token. */
					access_code = obtain_access_code( curl,
													  user_code->device_code,
											   global_config.client_id,
											   global_config.client_password );
					if( access_code != NULL )
					{
						g_printf( "Access token: %s\n", access_code->access_token );
						exit(1);
					}
				}
			}
		}
	}
}

//https://www.googleapis.com/oauth2/v1/userinfo?access_token=ya29.AHES6ZTwO4mSEyPNmx-b5rlA0dBb3qUD14OGWFYdEYa_2Wk

//https://www.googleapis.com/tasks/v1/users/@me/lists?access_token=ya29.AHES6ZTwO4mSEyPNmx-b5rlA0dBb3qUD14OGWFYdEYa_2Wk
