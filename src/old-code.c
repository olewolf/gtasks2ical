/**
 * \file old-code.c
 * \brief Code that is not currently in use.
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


#if 0 /* Disabled; was an unnecessary attempt at raw HTTP. */

/*
 * Auxiliary function that adds an input element name and value to an HTML
 * body buffer.
 * @param input_field_ptr [in] Pointer to an input_field_t structure with
 *        input name and value.
 * @param body_ptr [in/out] Pointer to \a curl_write_buffer_t structure
 *        where the data is written.
 * @return Nothing.
 */
STATIC void
fill_body_with_input( gpointer input_field_ptr, gpointer body_ptr )
{
	const input_field_t        *input = input_field_ptr;
	struct curl_write_buffer_t *body  = body_ptr;
	const gchar                *name;
	const gchar                *value;

	name  = input->name;
	value = input->value;

	/* The receive_curl_response copies data to an automatically expanding
	   buffer, which is exactly what we need. */
	receive_curl_response( name, strlen( name ), 1, body );
	receive_curl_response( "=", strlen( "=" ), 1, body );
	if( value != NULL )
	{
		receive_curl_response( value, strlen( value ), 1, body );
	}
	receive_curl_response( "\n", strlen( "\n" ), 1, body );
}



/**
 * Wait for a CURL socket to become available for sending or receiving data.
 * @param socket [in] CURL socket to wait for.
 * @param to_receive [in] If \a TRUE, wait for the socket to become ready
 *        for receiving data.  If \a FALSE, wait for it to become ready for
 *        sending data.
 * @param timeout [in] Timeout in milliseconds.
 * @return \a TRUE if the socket is available, or \a FALSE on timeout.
 */
STATIC gboolean
wait_for_socket( curl_socket_t socket, gboolean to_receive, long timeout )
{
	struct timeval tv;
	fd_set         receive_fd, send_fd, error_fd;

	tv.tv_sec  = timeout / 1000;
	tv.tv_usec = ( timeout % 1000 ) * 1000;

	FD_ZERO( &receive_fd );
	FD_ZERO( &send_fd );
	FD_ZERO( &error_fd );

	FD_SET( socket, &error_fd );

	if( to_receive == TRUE )
	{
		FD_SET( socket, &receive_fd );
	}
	else
	{
		FD_SET( socket, &send_fd );
	}

	/* select() returns the number of signaled sockets or -1 */
	return( select( socket + 1, &receive_fd, &send_fd, &error_fd, &tv ) );
}


#pragma GCC diagnostic ignored "-Wformat-contains-nul"
STATIC gchar*
post_rest_form( CURL *curl, const form_field_t *form )
{
	gchar                      *url_host;
	gchar                      *url_path;
	gchar                      *headers;
	struct curl_write_buffer_t body            = { .data = NULL, .size = 0 };
	gchar                      content_length[ 10 ];
	gchar                      *html_post;
	GRegex                     *url_split_regex;
	GMatchInfo                 *match_info;

	long                       socket_value;
	curl_socket_t              socket;
	size_t                     bytes_sent;
	char                       response_buffer[ 1024 ];
	CURLcode                   status;
	size_t                     bytes_received;
	gboolean                   read_error;

		/* Split the form action URL into host and path. */
		url_split_regex = g_regex_new( "^http[s?]://([a-zA-Z0-9_\\.-]+)/(.*)$",
									   0, 0, NULL );
		if( g_regex_match( url_split_regex, form_action, 0, &match_info ) )
		{
			url_host = g_match_info_fetch( match_info, 1 );
			url_path = g_match_info_fetch( match_info, 2 );
			g_match_info_free( match_info );
			g_regex_unref( url_split_regex );

			/* Build the HTML post. */
			if( url_host != NULL )
			{
				/* Build the body. */
				if( form_name != NULL )
				{
					body.data = g_strconcat( form_name, "=", form_value, NULL );
					body.size = strlen( body.data );
				}
				g_slist_foreach( form->input_fields, fill_body_with_input,
								 &body );

				/* Build the headers. */
				g_sprintf( content_length, "%d\n\0", (gint) body.size );
				headers = g_strconcat(
					"POST ", url_path, " HTTP/1.1\n",
					"Host: ", url_host, "\n",
					"Content-Type: application/x-www-form-urlencoded\n",
					"Content-Length: ", content_length,
					NULL );
				g_free( url_path );
				g_free( url_host );

				/* Combine the headers and the body. */
				html_post = g_strconcat( headers, "\n", body.data, NULL );
				g_free( body.data );
				g_free( headers );

				printf( "DEBUG: Sending:\n-------\n%s-------", html_post );
				/* Prepare a connection. */
				curl_easy_setopt( curl, CURLOPT_URL, form_action );
				curl_easy_setopt( curl, CURLOPT_CONNECT_ONLY, 1 );
				curl_easy_perform( curl );
				curl_easy_getinfo( curl, CURLINFO_LASTSOCKET, &socket_value );
				socket = socket_value;
				/* Wait for the socket to become ready for sending. */
				if( wait_for_socket( socket, FALSE, 60000L ) == FALSE )
				{
					g_free( html_post );
					g_fprintf( stderr, "Timeout connecting to "
							   "the Google REST service\n" );
				}
				else
				{
					/* Submit the post. */
					curl_easy_send( curl, html_post, strlen( html_post ),
									&bytes_sent );
					/* To do: verify that all the bytes were sent; if not,
					   send the remainding bytes in a second, third, ...
					   call. */
					printf( "DEBUG: Sent %d out of %d bytes\n", (int)bytes_sent, (int) strlen(html_post) );
					g_free( html_post );

					/* Receive the response. */
					read_error = FALSE;
					do
					{
						/* Wait for the socket to become ready for receiving. */
						if( wait_for_socket( socket, TRUE, 60000L ) == FALSE )
						{
							read_error = TRUE;
							g_fprintf( stderr, "Timeout connecting to "
									   "the Google REST service\n" );
							break;
						}
						/* Copy chunks of data into html_response. */
						else
						{
							status = curl_easy_recv( curl, response_buffer,
													 sizeof( response_buffer ),
													 &bytes_received );
							if( status == CURLE_OK )
							{
								receive_curl_response( response_buffer,
													   bytes_received, 1,
													   &html_response );
							}
						}
					} while( ( status == CURLE_AGAIN ) &&
							 ( read_error == FALSE ) );
					printf( "DEBUG: CURL code %d: %s\n", (int)status, curl_easy_strerror( status ) );
				}
			}
		}
}
#endif



#if 0
/**
 * Auxiliary function used by decode_user_code_response to copy a JSON
 * response into a local data structure.
 * @param node [in] Object with JSON data (unused).
 * @param member_name [in] Name of the JSON field.
 * @param member_node [in] Node with JSON data.
 * @param user_code_ptr [out] Pointer to the structure where the JSON
 *        data will be stored.
 * @return Nothing.
 */
STATIC void
parse_user_code_json( const gchar *member_name,
					  JsonNode *member_node, gpointer user_code_ptr )
{
	struct user_code_t *user_code = user_code_ptr;

	SET_JSON_MEMBER( user_code, device_code );
	SET_JSON_MEMBER( user_code, verification_url );
	SET_JSON_MEMBER( user_code, user_code );
}
#endif




/**
 * Decode the initial OAuth2 response with device code, user code, and
 * verification URL.
 * @param user_code_response [in] Raw JSON response data.
 * @return \a user_code_t structure with authentication data, or \a FALSE
 *         if the user code could not be obtained.
 */
#if 0
STATIC struct user_code_t*
decode_user_code_response( const gchar *user_code_response )
{
	struct user_code_t *user_code;

	user_code = g_new0( struct user_code_t, 1 );

	if( user_code_response != NULL )
	{
		/* Decode the JSON response. */
		decode_json_reply( user_code_response, parse_user_code_json,
						   user_code );
		/* Verify that all fields have been decoded. */
		if( ( user_code->device_code == NULL ) ||
			( user_code->user_code == NULL ) ||
			( user_code->verification_url == NULL ) )
		{
			g_free( (gchar*) user_code->device_code );
			g_free( (gchar*) user_code->user_code );
			g_free( (gchar*) user_code->verification_url );
			g_free( user_code );
			user_code = NULL;
		}
	}

	return( user_code );
}
#endif



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
	/* Note: the following code is more elegant than the code that is
	   currently used but is disabled because a bug in Google's API system
	   prevents devices from using the tasks scope. */
#if 0
	static gchar *form_action = 
		"https://accounts.google.com/o/oauth2/device/code";
	gchar *scope =
//		"https://www.googleapis.com/auth/userinfo.email "
//	    "https://www.googleapis.com/auth/userinfo.profile";
	    "https://www.googleapis.com/auth/tasks";
	form_field_t       form = { .name         = NULL,
								.value        = NULL,
								.action       = form_action,
								.input_fields = NULL };
	gchar              *user_code_response;
	struct user_code_t *user_code;
	gboolean           success;

	/* Create a user code request. */
	add_input_to_form( &form, "client_id", client_id );
	add_input_to_form( &form, "scope", scope );

	/* Post the request and decode the response. */
	user_code_response = post_form( curl, &form );
	destroy_form_inputs( &form );
	g_printf( "%s\n", user_code_response );
	user_code = decode_user_code_response( user_code_response );
	return( user_code );
#endif
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





		/* As of October 1, 2012, there is a bug in Google's authentication
		   API that prevents devices from accessing the Tasks scope.  To
		   work around this, determine whether an access token is required. */
		if( user_code->verification_url != NULL )
		{
			success = FALSE;

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
			}
		}
