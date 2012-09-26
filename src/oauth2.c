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
#include <tidy/tidy.h>
#include <tidy/buffio.h>
#include <libxml/parser.h>
#include <curl/curl.h>
#include <string.h>
#include "gtasks2ical.h"
#include "oauth2.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


/* File-scope function prototypes. */
STATIC GSList *get_forms( const gchar *raw_html_page );




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
search_input_by_name( gconstpointer input_ptr,
					  gconstpointer input_names_ptr )
{
	const input_field_t *input        = input_ptr;
	const gchar *const  *input_names = input_names_ptr;
	gint                idx           = 0;
	gint                name_found    = 1;

	/* Find out if the input element is named according to the list of input
	   names. */
	while( input_names[ idx ] )
	{
		/* Set the "found" flag if the name matches. */
		if( g_strcmp0( input->name, input_names[ idx ] ) == 0 )
		{
			name_found = 0;
			break;
		}
		idx++;
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
	GSList                     *input_fields;
	GSList                     *input;
	gint                       form_found = 1;
	size_t                     max_action_chars;

	max_action_chars = strlen( search->form_action );
	if( strncmp( form->action, search->form_action, max_action_chars  ) == 0 )
	{
		/* If the form action was found, determine whether the input fields
		   match, too. */
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
 * Retrieve the names and values of all the input elements contained in an
 * HTML form node.
 * @param form_node [in] XML node of the form.
 * @return List of input fields.
 * Test: implied (test-oauth2.c: get_forms).
 */
STATIC GSList*
get_input_fields( const xmlNode *form_node )
{
	const xmlNode *node;
	GSList        *input_fields = NULL;
	input_field_t *new_input_field;
	xmlChar       *name;
	xmlChar       *value;

	for( node = form_node->xmlChildrenNode; node; node = node->next )
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
			/* Otherwise, if a form was not found, go to the next level of the
			   xml structure. */
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
 * Callback function for a CURL write.  The function maintains a dynamically
 * expanding buffer that may be filled successively by multiple callbacks.
 * @param ptr [in] Source of the data from CURL.
 * @param size [in] The size of each data block.
 * @param nmemb [in] Number of data blocks.
 * @param state_data [in] Passed from the CURL write-back as a pointer to a
 *        \a struct \a curl_write_buffer_t.  It must be initialized to
 *        all-zeroes before use.
 * @return Number of bytes copied from \a ptr.
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
 */
STATIC gchar*
read_url( CURL *curl, const gchar *url )
{
	struct curl_write_buffer_t html_body = { NULL, 0 };

	/* Receive the HTML body only, using receive_curl_response to store it
	   in html_body. */
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_body );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
	/* Set the URL. */
	curl_easy_setopt( curl, CURLOPT_URL, url );
	/* Send the request, receiving the response in html_body. */
	curl_easy_perform( curl );
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



STATIC void
fill_form_with_input( gpointer input_field_ptr, gpointer form_vars_ptr )
{
	input_field_t           *input_field = input_field_ptr;
	struct curl_form_vars_t *form_vars   = form_vars_ptr;
	const gchar *name;
	const gchar *value;

	name  = input_field->name;
	value = input_field->value;
	curl_formadd( form_vars->login_form_post, form_vars->last_post_data,
				  CURLFORM_COPYNAME, name,
				  CURLFORM_COPYCONTENTS, value, CURLFORM_END );
}



/**
 * Login to Google Tasks.  This function obtains the token required to
 * access the Google Tasks.
 * @param username [in] The user's Google username (username@gmail.com).
 * @param password [in] The user's Google password.
 * @return Access token, or \a NULL if the login failed.
 */
gchar*
login_to_tasks( CURL *curl, const gchar *username, const gchar *password )
{
	const gchar  *login_url = "https://accounts.google.com/ServiceLogin?"
		"passive=true&rm=false&"
		"continue=https://mail.google.com/mail/";
	const gchar  *login_action = "https://accounts.google.com/ServiceLoginAuth";
	const gchar  *login_inputs[ ] = { "Email", "Passwd", NULL };
	form_field_t *login_form;

	struct curl_write_buffer_t html_body        = { NULL, 0 };
	struct curl_httppost       *login_form_post = NULL;
	struct curl_httppost       *last_post_data  = NULL;
	const gchar                *form_name;
	const gchar                *form_value;
	struct curl_form_vars_t    curl_form_vars   = { &login_form_post,
													&last_post_data };
	struct curl_slist          *curl_headers;
	const gchar                *form_action;

	/* Get the original login form. */
	login_form = get_form_from_url( curl, login_url, login_action,
									login_inputs );

	/* Submit the login form. */
	curl_easy_setopt( curl, CURLOPT_POST, 1 );
	/* Add form and input names and values. */
	form_name  = login_form->name;
	form_value = login_form->value;
	curl_formadd( &login_form_post, &last_post_data,
				  CURLFORM_COPYNAME, form_name,
				  CURLFORM_COPYCONTENTS, form_value, CURLFORM_END );
	g_slist_foreach( login_form->input_fields, fill_form_with_input,
					 &curl_form_vars );
	/* Expect: 100-continue is unwanted. */
	curl_headers = curl_slist_append( NULL, "Expect:" );
	/*  Post the form using receive_curl_response to store the reply. */
	curl_easy_setopt( curl, CURLOPT_HTTPPOST, login_form_post );
	curl_easy_setopt( curl, CURLOPT_WRITEDATA, &html_body );
	curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, receive_curl_response );
	curl_easy_setopt( curl, CURLOPT_HEADERDATA, NULL );
	curl_easy_setopt( curl, CURLOPT_HEADERFUNCTION, NULL );
	/* Set the URL. */
	form_action = login_form->action;
	curl_easy_setopt( curl, CURLOPT_URL, form_action );
	/* Send the request, receiving the response in html_body. */
	curl_easy_perform( curl );
	curl_formfree( login_form_post );
	curl_slist_free_all( curl_headers );
	curl_easy_reset( curl );

	return( html_body.data );


}

