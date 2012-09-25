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


/**
 * Retrieve the names and values of all the input elements contained in an
 * HTML form node.
 * @param form_node [in] XML node of the form.
 * @return List of input fields.
 * Test: implied (test-oauth.c: get_forms).
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
 * Recursive function that builds a list of forms including their input
 * elements.
 * @param node [in] Current depth of the XML document.
 * @param form_fields [in] Forms list to which the forms will be appended.
 * @return New value for the forms list.
 * Test: implied (test-oauth.c: get_forms).
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
 * Test: unit test (test-oauth.c: get_forms).
 */
GSList*
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
	   libtidy and then removing CDATA it can be parsed. Perform some other
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
		printf( "ERROR: cannot parse the Google HTML response\n" );
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

