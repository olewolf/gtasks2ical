/**
 * \file merge.h
 * \brief Merge iCalendar todos and Google Tasks.
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

#ifndef __MERGE_TASKS_H
#define __MERGE_TASKS_H

#include <config.h>
#include <glib.h>
#include <libical/ical.h>
#include "gtasks2ical.h"
#include "gtasks.h"



struct geo_location_t
{
	double latitude;
	double longitude;
};

struct recurrence_id
{
	gchar     *uid;
	GDateTime *dtstart;
	GDateTime *range;
	gint      sequence;
};

struct x_google_task_link_t
{
	gchar *type;
	gchar *description;
	gchar *url;
};

enum status_t
{
	STATUS_NEEDS_ACTION = 1,
	STATUS_COMPLETED,
	STATUS_IN_PROCESS,
	STATUS_CANCELLED
};


/*
 * Container for both standard iCalendar fields and Google Task specific
 * fields.  Fields that may occur more than once are defined as lists.
 */
typedef struct 
{
	gchar                       *uid;
	gchar                       *url;
	gchar                       *x_google_task_url;
	gchar                       *x_google_task_id;

	gint                        seq;
	GDateTime                   *last_modified;

	GSList                      *comment;
	gchar                       *description;
	gchar                       *title;

	gchar                       *class;
	gint                        priority;
	enum status_t               status;
    gint                        percent;
	gchar                       *organizer;
	gchar                       *contact;
	gchar                       *location;
	struct geo_location_t       geo;

	GDateTime                   *created;
	GDateTime                   *dtstamp;
	GDateTime                   *dtstart;
	GDateTime                   *due;
	GDateTime                   *duration;
	GDateTime                   *completed;

	GSList                      *related;
	gchar                       *x_google_task_position;
	GSList                      *resources;

	GSList                      *attendee;
	GSList                      *request_status;

	GSList                      *exdate;
	GSList                      *exrule;

	struct recurrence_id        recurrence;
	GSList                      *rdate;
	GSList                      *rrule;
	GSList                      *rstatus;

	gboolean                    x_google_task_deleted;
	gboolean                    x_google_task_hidden;

	GSList                      *attach;
    /* The links are obvious as attachments, but I need to figure out
	   how to add the link description to the attachment.
	GSList                      *x_google_task_links;
	*/
} unified_task_t;


struct merged_tasks_t
{
	GTree  *merged_tasks;
	GSList *unmatched_icalendar_todos;
	GSList *unmatched_google_tasks;
	GSList *problems;
};


#endif /* __MERGE_TASKS_H */
