/**
 * \file oauth2-google.h
 * \brief Definitions for Google's OAuth2.
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


/* URLs for submitting requests.. */
#define GOOGLE_GMAIL_LOGIN "https://accounts.google.com/ServiceLogin"
#define GOOGLE_OAUTH2_DEVICECODE "https://accounts.google.com/o/oauth2/auth"
#define GOOGLE_OAUTH2_APPROVAL "https://accounts.google.com/o/oauth2/approval"
#define GOOGLE_OAUTH2_TOKEN "https://accounts.google.com/o/oauth2/token"


/* Structure used in auxiliary search functions to pass multiple values as
   one parameter. */
struct form_search_t
{
	const gchar        *form_action;
	const gchar *const *input_names;
};


/* Define a custom data type that is used by a JSON foreach callback to
   decode the initial OAuth2 response. */
struct user_code_t
{
	const gchar *device_code;
	const gchar *user_code;
	const gchar *verification_url;
};


/* Custom data type containing the access token, the refresh token, and the
   timeout timestamp of the access token. */
typedef struct
{
	gchar  *access_token;
	gchar  *refresh_token;
	time_t expiration_timestamp;
} access_code_t;


/* Login to the user's Gmail account, keeping the login state in the CURL
   session. */
gboolean login_to_gmail( CURL *curl, const gchar *username,
						 const gchar *password );
/* Authorize the application to access the user's Google Tasks. */
access_code_t *authorize_application( CURL *curl );



#endif /* __GTASKS_OAUTH2_H */
