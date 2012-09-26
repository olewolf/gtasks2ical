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
#include <stdlib.h>
#include <glib.h>


/* To use gtasks2ical in your own application, register your application
   with Google and change the client ID and client secret settings to match
   those of your application. */
#define GOOGLE_CLIENT_ID     430385316884.apps.googleusercontent.com
#define GOOGLE_CLIENT_SECRET Fcjb9cdutlDWu2lSLq2IaC5M

/* Define file scope functions as global during testing. */
#ifdef AUTOTEST
#define STATIC
#else
#define STATIC static
#endif


struct Configuration
{
	gchar *listname;
	gchar *in_ical_filename;
	gchar *out_ical_filename;

	gchar *google_username;
	gchar *google_password;

	gboolean verbose;
};


gboolean
Initialize_Configuration(
    struct Configuration *configuration, int argc, const char *const *argv );

gboolean
Google_Authenticate( gchar *username, gchar *password );




#endif /* __GTASKS2ICAL_H */
