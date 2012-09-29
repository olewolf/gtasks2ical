/**
 * \file oauth2.h
 * \brief Definitions for OAuth2.
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

#ifndef __GTASKS_OAUTH2_H
#define __GTASKS_OAUTH2_H


#include <config.h>
#include <glib.h>


/* URL for submitting an approval form. */
#define GOOGLE_OAUTH2_APPROVAL "https://accounts.google.com/o/oauth2/approval"


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


/* Structure used in auxiliary search functions to pass multiple values as
   one parameter. */
struct form_search_t
{
	const gchar        *form_action;
	const gchar *const *input_names;
};


/* Define a custom data type that is passed by the CURL call-back for
   writing data received from Google. */
struct curl_write_buffer_t
{
	gchar *data;
	gsize size;
};


/* Define a custom data type that is used by a JSON foreach callback to
   decode the initial OAuth2 response. */
struct initial_oauth2_t
{
	const gchar *device_code;
	const gchar *user_code;
	const gchar *verification_url;
};



/* Login to the user's Gmail account, keeping the login state in the CURL
   session. */
gboolean login_to_gmail( CURL *curl, const gchar *username,
						 const gchar *password );
/* Get the device code. */
gchar *obtain_device_code( CURL *curl, const gchar *client_id );



#endif /* __GTASKS_OAUTH2_H */
