/**
 * \file gtasks2ical.h
 * \brief Main entry for converting between iCalendar and Google Tasks.
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

#ifndef __GTASKS2ICAL_H
#define __GTASKS2ICAL_H


#include <config.h>
#include <glib.h>


/* Set the environment variable name that contains the Gmail credentials,
   formatted as username@gmail.com:password. */
#define GMAIL_CREDENTIALS "GMAIL_CREDENTIALS"
/* Set the environment variable name that contains the Client ID and password,
   formatted as clientid:clientpassword. */
#define GTASKS2ICAL_CLIENT "client-id:client-password"

/* Name of the global configuration file relative to the system configuration
   directory (which is typically either /etc or /usr/local/etc). */
#define CONF_FILE_NAME /gtasks2ical.conf
/* Name of the local configuration file relative to the user's home
   directory. */
#define LOCAL_CONF_FILE_NAME /.gtasks2icalrc


/* Define file scope functions as global during testing. */
#ifdef AUTOTEST
#define STATIC
#else
#define STATIC static
#endif


struct configuration_t
{
	gchar    *configuration_file;

	gchar    *listname;
	gchar    *ical_filename;
	gboolean force_upload;
	gboolean force_download;
	GSList   *tasks;

	gchar    *gmail_username;
	gchar    *gmail_password;
	gchar    *client_id;
	gchar    *client_password;

	gboolean verbose;
	gboolean ipv4_only;
};


gboolean
initialize_configuration(
    struct configuration_t *configuration, int argc, const char *const *argv );


#endif /* __GTASKS2ICAL_H */
