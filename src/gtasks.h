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


typedef struct
{
	gchar     *id;
	gchar     *title;
	GDateTime *updated;
} gtask_list_t;


typedef struct
{
	gchar *type;
	gchar *description;
	gchar *link;
} gtask_link_t;

typedef struct
{
	gchar     *id;
	gchar     *x_google_task_id;
	gchar     *etag;
	gchar     *title;
	GDateTime *updated;
	gchar     *self_link;
	gchar     *parent;
	gchar     *position;
	gchar     *notes;
	gchar     *status;
	GDateTime *due;
	GDateTime *completed;
	gboolean  deleted;
	gboolean  hidden;
	GSList    *links;
} gtask_t;




/*
 * Read the user's task lists.
 */
GSList* get_gtasks_lists( CURL *curl, const gchar *access_token );
gtask_list_t* get_specified_gtasks_list( CURL *curl, const gchar *access_token,
										 const char *task_list_name );
/*
 * Read tasks from a specified list.
 */
GSList* get_all_list_tasks( CURL *curl, const gchar *access_token,
							const char *task_list_id, const char *page_token );
gtask_t* get_specified_task( CURL *curl, const gchar *access_token,
							 const gchar *task_list_id, const gchar *task_id );


#endif /* __GTASKS_H */
