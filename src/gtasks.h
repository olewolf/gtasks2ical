/**
 * \file gtasks.h
 * \brief Definitions for Google's Tasks API.
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

#ifndef __GTASKS_H
#define __GTASKS_H

#include <config.h>
#include <glib.h>
#include <curl/curl.h>

#define GOOGLE_TASKS_API "https://www.googleapis.com/tasks/v1/"


/*
 * Read the user's task lists.
 */
GSList* get_gtasks_lists( CURL *curl, const gchar *access_token );



#endif /* __GTASKS_H */
