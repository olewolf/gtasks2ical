/**
 * \file oauth2.c
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
#include "gtasks2ical.h"
#include "oauth2.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


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
		printf( "ERROR: cannot parse the HTML document\n" );
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
		fprintf( stderr, "Could not receive web page" );
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
		new_input = g_new( input_field_t, 1 );
		new_input->name  = g_strdup( input_field->name );
		new_input->value = g_strdup( input_field->value );
		form->input_fields = g_slist_append( form->input_fields, new_input );
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
 * Submit a form according to its \a action attribute, and receive the response
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
	struct curl_httppost       *form_post[ 2 ] = { NULL, NULL };
	struct curl_write_buffer_t html_body       = { NULL, 0 };
	struct curl_slist          *curl_headers   = NULL;

	/* Copy the form. */
	form_name  = form->name;
	form_value = form->value;
	curl_formadd( &form_post[ 0 ], &form_post[ 1 ],
				  CURLFORM_COPYNAME, form_name,
				  CURLFORM_COPYCONTENTS, form_value,
				  CURLFORM_END );
	/* Copy the form's input elements. */
	g_slist_foreach( form->input_fields, fill_form_with_input, form_post );

	/*  Post the form using receive_curl_response to store the reply. */
	curl_easy_setopt( curl, CURLOPT_POST, 1 );
	curl_easy_setopt( curl, CURLOPT_HTTPPOST, form_post[ 0 ] );
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_body );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
//	curl_easy_setopt( curl, CURLOPT_VERBOSE, 1 );
	/* We may be redirected several times. */
	curl_easy_setopt( curl, CURLOPT_FOLLOWLOCATION, 1 );
	curl_easy_setopt( curl, CURLOPT_UNRESTRICTED_AUTH, 1 );
	/* Set the URL. */
	form_action = form->action;
	curl_easy_setopt( curl, CURLOPT_URL, form_action );
	/* "Expect: 100-continue" is unwanted. */
	curl_headers = curl_slist_append( NULL, "Expect:" );
	curl_easy_setopt( curl, CURLOPT_HTTPHEADER, curl_headers );
	if( global_config.ipv4_only == TRUE )
	{
		curl_easy_setopt( curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4 );
	}
	/* Send the request, receiving the response in html_body. */
	curl_easy_perform( curl );
	curl_formfree( form_post[ 0 ] );
	curl_slist_free_all( curl_headers );
	curl_easy_reset( curl );
	return( html_body.data );
}



/**
 * Login to Gmail.  This function attempts to pass the first part of the
 * login where the user is requested to enter his/her username and password.
 * @param username [in] The user's Google username (username@gmail.com).
 * @param password [in] The user's Google password.
 * @return \a TRUE if the login was successfull, or \a FALSE otherwise.
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

	#ifdef AUTOTEST
	if( login_response != NULL )
	{
		printf( "%s\n", login_response );
	}
	#endif

	/* Determine whether some typical login page content is present in the
	   response page. */
	response_length = strlen( login_response );
	if( g_strstr_len( login_response, response_length,
					  "https://www.google.com/settings/ads/preferences" ) &&
		g_strstr_len( login_response, response_length,
					 "href=\"https://plus.google.com/u/0/me\"" ) )

/*
	if( ( g_strcmp0( login_response, "https://plus.google.com/u/0/me" ) == 0 ) &&
		( g_strcmp0( login_response, "href=\"https://accounts.google.com/AddSession" ) == 0 ) &&
		( g_strcmp0( login_response, "continue=https://mail.google.com/mail/u/0/&service=mail" ) == 0 ) &&
		( g_strcmp0( login_response, "\"GMAIL_CB\",GM_START_TIME" ) == 0 ) )
*/
	{
		logged_in = TRUE;
	}
	else
	{
		logged_in = FALSE;
	}

	return( logged_in );
}



#if 0
/**
 * Auxiliary function used by decode_initial_oauth2_response to copy a JSON
 * response into a local data structure.
 * @param node [in] Object with JSON data (unused).
 * @param member_name [in] Name of the JSON field.
 * @param member_node [in] Node with JSON data.
 * @param initial_oauth2_ptr [out] Pointer to the structure where the JSON
 *        data will be stored.
 * @return Nothing.
 */
STATIC void
parse_initial_oauth2_json( JsonObject *node, const gchar *member_name,
						   JsonNode *member_node, gpointer initial_oauth2_ptr )
{
	struct initial_oauth2_t *initial_oauth2 = initial_oauth2_ptr;

	if( g_strcmp0( member_name, "device_code" ) == 0 )
	{
		initial_oauth2->device_code = json_node_dup_string( member_node );
	}
	else if( g_strcmp0( member_name, "verification_url" ) == 0 )
	{
		initial_oauth2->verification_url = json_node_dup_string( member_node );
	}
	else if( g_strcmp0( member_name, "user_code" ) == 0 )
	{
		initial_oauth2->user_code = json_node_dup_string( member_node );
	}
	else
	{
		/* Ignore other JSON data. */
	}
}



/**
 * Decode the initial OAuth2 response with device code, user code, and
 * verification URL.
 * @param user_code_response [in] Raw JSON response data.
 * @param initial_oauth2 [out] Structure where the data is written.
 * @return \a TRUE if the response was successfully decoded, or \a FALSE
 *         otherwise.
 */
STATIC gboolean
decode_initial_oauth2_response( const gchar *user_code_response,
								struct initial_oauth2_t *initial_oauth2 )
{
	JsonParser *json_parser;
	JsonNode   *node;
	JsonObject *root;
	gboolean   success;

	success = FALSE;
	initial_oauth2->device_code      = NULL;
	initial_oauth2->user_code        = NULL;
	initial_oauth2->verification_url = NULL;
	if( user_code_response != NULL )
	{
		/* Walk through the JSON response, beginning at the root. */
		json_parser = json_parser_new( );
		json_parser_load_from_data( json_parser, user_code_response,
									strlen( user_code_response ), NULL );
		node = json_parser_get_root( json_parser );
		root = json_node_get_object( node );
		json_object_foreach_member( root, parse_initial_oauth2_json, NULL );
		/* Release parser memory. */
		g_object_unref( json_parser );

		/* Verify that all fields have been decoded. */
		if( ( initial_oauth2->device_code != NULL ) &&
			( initial_oauth2->user_code != NULL ) &&
			( initial_oauth2->verification_url != NULL ) )
		{
			success = TRUE;
		}
	}

	/* Free memory on failure. */
	if( success == FALSE )
	{
		g_free( (gchar*) initial_oauth2->device_code );
		g_free( (gchar*) initial_oauth2->user_code );
		g_free( (gchar*) initial_oauth2->verification_url );
	}

	return( success );
}
#endif



/**
 * Obtain a device code for device access.  The CURL session must include the
 * user's login to Google, which may be obtained by calling \a login_to_gmail
 * with the user's credentials.
 * @param curl [in] CURL handle.
 * @param client_id [in] Google Developer Client ID.
 * @return Device code or \a NULL if an error occurred.
 */
gchar*
obtain_device_code( CURL *curl, const gchar *client_id )
{
	static const gchar *endpoint     =
		"https://accounts.google.com/o/oauth2/auth";
	static const gchar *redirect_uri = "urn:ietf:wg:oauth:2.0:oob";
	static const gchar *scope        = "https://www.googleapis.com/auth/tasks";
	gchar              *request_url;
	struct initial_oauth2_t *initial_oauth2;
	form_field_t       *authorization_form;
	static const gchar *auth_form_action =
		"https://accounts.google.com/o/oauth2/approval";
	static const gchar *auth_input_names[ ] = { "submit_access", NULL };
	gchar              *auth_response;
	GRegex             *code_regex;
	GMatchInfo         *match_info;
	gchar              *code;

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
			code = g_match_info_fetch( match_info, 1 );
		}
		else
		{
			code = NULL;
		}
		g_match_info_free( match_info );
		g_regex_unref( code_regex );
	}
	else
	{
		code = NULL;
	}

	return( code );

#if 0
	/* Disabled code for OAuth2 which for some reason won't work with
	   my particular client ID. */

	static gchar *form_action = 
		"https://accounts.google.com/o/oauth2/device/code";
	gchar *scope =
//		"https://www.googleapis.com/auth/tasks";
//		"https://www.googleapis.com/auth/userinfo.email "
//		"https://www.googleapis.com/auth/userinfo.profile";
	form_field_t  form            = { .name         = NULL,
									  .value        = NULL,
						              .action       = form_action,
									  .input_fields = NULL };
	input_field_t input_client_id = { .name = "client_id", .value = NULL };
	input_field_t input_scope     = { .name = "scope", .value = scope };
	gchar                   *user_code_response;
	struct initial_oauth2_t *initial_oauth2;
	gboolean                success;

	input_client_id.value = g_strdup( "812741506391-h38jh0j4fv0ce1krdkiq0hfvt6n5amrf.apps.googleusercontent.com" );

	/* Create a user code request. */
	form.input_fields = g_slist_append( form.input_fields, &input_client_id );
	form.input_fields = g_slist_append( form.input_fields, &input_scope );

	/* Post the request and decode the response. */
	user_code_response = post_form( curl, &form );
	printf( "%s\n", user_code_response );
	g_slist_free( form.input_fields );
	initial_oauth2 = g_new( struct initial_oauth2_t, 1 );
	exit( 1 );
	success = decode_initial_oauth2_response( user_code_response,
											  initial_oauth2 );

	/* Return NULL if an error occurred. */
	if( success == FALSE )
	{
		g_free( initial_oauth2 );
		initial_oauth2 = NULL;
	}
#endif
}

