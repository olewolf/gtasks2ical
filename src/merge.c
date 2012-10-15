/**
 * \file merge.c
 * \brief Merge iCalendar todos and Google Tasks.
 *
 * The data fields are mapped as follows (where fields marked with an
 * asterisk may occur more than once):
 *
 * ------------------------       ------------------------
 * Google Task                    iCal
 * ------------------------       ------------------------
 * -                              attach*
 * -                              attendee*
 * -                              categories*
 * -                              class
 * -                              comment*
 * completed                      completed
 * -                              contact*
 * -                              created
 * notes                          description
 * -                              dtstamp
 * -                              dtstart (i.e., start date)
 * due                            due
 * -                              duration
 * -                              exdate*
 * -                              exrule*
 * -                              geo
 * updated                        last-mod
 * -                              location
 * [gmail login]                  organizer
 * -                              percent
 * -                              priority
 * -                              rdate*
 * -                              recurid
 * -                              related*
 * -                              request-status*
 * -                              resources*
 * -                              rrule*
 * -                              rstatus*
 * -                              seq
 * status                         status
 * title                          summary
 * id(1)                          uid
 * selfLink(1)                    url
 *
 * id(1)                          x-google-task-id
 * selfLink(1)                    x-google-task-url
 * parent                         x-google-task-parent
 * position                       x-google-task-position
 * deleted                        x-google-task-deleted
 * hidden                         x-google-task-hidden
 * links.type                     x-google-task-linktypes*
 * links.description              x-google-task-linkdescriptions*
 * links.link                     x-google-task-links*
 * ------------------------       ------------------------
 * 
 * (1) The Google Task ID and the self-link cannot be overwritten.  If the
 *     the iCalendar task is created first, thus making it impossible to
 *     replicate its UID and URL in these fields, the Google Task ID and the
 *     self-link are be stored in x-google-task-id and x-google-task-url
 *     instead.  (If the iCalendar does not specify the UID field or the URL
 *     fields, it may be used to store the Google Task value once it is known.)
 *
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
#include <string.h>
#include <glib/gprintf.h>
#include <libical/ical.h>
#include "gtasks2ical.h"
#include "gtasks.h"

#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


typedef struct
{
	gchar *id;
} icalendar_t;


/* Global configuration data. */
extern struct configuration_t global_config;


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


/* Pointers to matching task and todo entries.  If one pointer is NULL, then
   the other task/todo has no corresponding todo/task. */
typedef struct
{
	gtask_t     *google_task;
	icalendar_t *ical_todo;
} match_t;

struct merged_tasks_t
{
	GTree  *merged_tasks;
	GSList *unmatched_icalendar_todos;
	GSList *unmatched_google_tasks;
	GSList *problems;
};

struct match_pair_search_t
{
	struct merged_tasks_t merged_tasks;
	GTree                 *all_google_tasks;
};

struct problem_detection_t
{
	const gchar *id;
	gboolean    problem_detected;
};




/**
 * Copy the contents of a GDateTime structure to a new one by adding "0" to
 * the old one.
 * @param src [in] Original GDateTime structure.
 * @return New GDateTime structure.
 */
STATIC GDateTime*
copy_gdatetime( const GDateTime *src )
{
	GDateTime *new_gdatetime = g_date_time_add_years( (GDateTime*) src, 0 );
	return( new_gdatetime );
}












/**
 * Copy a link in a Google Task's list of links to our internal task format.
 * @param link_ptr [in] Pointer to a newly allocated link entry which is
 *        initialized with the contents of the current Google Task link.
 * @param list_ptr [in] Pointer to the Google Task's link list.
 * @return Nothing.
 */
STATIC void
copy_google_link( gpointer link_ptr, gpointer list_ptr )
{
	GSList       **link_list = list_ptr;
	gtask_link_t *link       = link_ptr;
	gtask_link_t *new_link   = g_new( gtask_link_t, 1 );

	new_link->type        = g_strdup( link->type );
	new_link->description = g_strdup( link->description );
	new_link->link        = g_strdup( link->link );
	*link_list = g_slist_append( *link_list, link_ptr );
}



/**
 * Convert a string value with the status of a task in Google-speak to an
 * enumerated value.
 * @param status_string [in] Status in Google's format.
 * @return Status as enumeration, or \a STATUS_NEEDS_ACTION if the Google
 *         string is unexpected.
 */
STATIC enum status_t
google_string_to_status( const gchar *status_string )
{
	enum status_t status;

	if( g_strcmp0( status_string, "needsAction" ) == 0 )
	{
	    status = STATUS_NEEDS_ACTION;
	}
	else if( g_strcmp0( status_string, "completed" ) == 0 )
	{
	    status = STATUS_COMPLETED;
	}
	/* Google currently only supports needs-action or completed. */
	else
	{
		g_printf( "Error: unexpected task status = \"%s\" from Google\n",
				  status_string );
		status = STATUS_NEEDS_ACTION;
	}

	return( status );
}



/**
 * Create a new Google Task structure based on the Google Task information.
 * This includes the initialization of the following fields which are not
 * supported natively by the Google Tasks format:  sequence, organizer.
 * @param google_task [in] The Google Task information retrieved from the
 *        Google Tasks.
 * @return Initialized unified_task structure.
 */
unified_task_t*
create_new_google_task( gtask_t *google_task )
{
	unified_task_t *new_task;

	new_task = g_new0( unified_task_t, 1 );

	/* Copy the Google Task ID, appending "@google.com" to indicate where
	   the ID comes from. */
	new_task->uid = g_strconcat( google_task->id, "@google.com", NULL );
	/* Copy the title. */
	new_task->title = g_strdup( google_task->title );
	/* Copy the description. */
	new_task->description = g_strdup( google_task->notes );
	/* Copy the URL. */
	new_task->url = g_strdup( google_task->self_link );
	/* Copy the update time. */
	new_task->last_modified = copy_gdatetime( google_task->updated );
	/* If a child task, copy the parent. */
	new_task->related = g_slist_append( new_task->related,
										g_strdup( google_task->parent ) );
	/* Copy the position in the list. */
	new_task->x_google_task_position = g_strdup( google_task->position );
	/* Copy the due date. */
	new_task->due = copy_gdatetime( google_task->due );
	/* Copy the status and completed fields. */
	new_task->status = google_string_to_status( google_task->status );
	new_task->completed = copy_gdatetime( google_task->completed );
	/* Copy the link list as attachments. */
	g_slist_foreach( google_task->links, copy_google_link, &new_task->attach );
	/* Set "deleted" and "hidden" flags. */
	new_task->x_google_task_deleted = google_task->deleted;
	new_task->x_google_task_hidden  = google_task->hidden;
	/* Ignore the etag, which cannot be modified and does matter to other
	   synchronization as long as we have the unique ID. */
	/* Indicate that a first significant change has been made. */
	new_task->seq = 1;

	return( new_task );
}





















/**
 * Merge an iCalendar task and a Google Task together.
 * @param ical_todo [in] iCalendar todo entry.
 * @param google_task [in] Google Task.
 * @return Merged task.
 */
unified_task_t*
merge_matching_tasks( const icalendar_t* ical_todo, const gtask_t *google_task )
{
    /* Determine which one is newer.  The Google Task includes the "updated"
	   field, which is set automatically when the user updates the task.  We
	   cannot rely on the last-mod field in the iCalendar todo set, however,
	   because it is not known whether this field was supported on the client
	   that performed the most recent update to the file.  Instead we rely on
	   the last modification time of the iCalendar file itself. */

	return( NULL );
}



/**
 * Determine if a task ID is flagged as a problem in the sync problem list.
 * @param problem_ptr [in] Pointer to a string with the ID of the problem task.
 * @param detector_ptr [in] Pointer to the structure where problem IDs are
 *        flagged as tasks.
 * @return Nothing.
 */
STATIC void
check_problem_list( gpointer problem_ptr, gpointer detector_ptr )
{
	const gchar                *problem_id       = problem_ptr;
	struct problem_detection_t *problem_detector = detector_ptr;

	if( g_strcmp0( problem_id, problem_detector->id ) == 0 )
	{
		problem_detector->problem_detected = TRUE;
	}
}



/**
 * Determine whether a particular Google Task ID is flagged as a sync problem.
 * @param problems [in] List of sync problems IDs.
 * @param google_task [in] Google Task that is examined for problem IDs.
 * @return \a TRUE if the Google Task is flagged as a problem, or \ a FALSE
 *         otherwise.
 */
STATIC gboolean
google_task_is_in_problems_list( GSList *problems, gtask_t *google_task )
{
	struct problem_detection_t problem_detector;

	/* Search the problems list for the google task ID. */
	problem_detector.id               = google_task->id;
	problem_detector.problem_detected = FALSE;
	g_slist_foreach( problems, check_problem_list, &problem_detector );

	return( problem_detector.problem_detected );
}



/**
 * Add a Google Task which is not yet matched against an iCalendar todo and
 * which is not flagged as a problem to a list of unmatched Google Tasks.
 * @param key [in] Unused.
 * @param google_task_ptr [in] Pointer to the Google Task that is a candidate
 *        for the unmatched list.
 * @param matches_ptr [in] Pointer to the list of unmatched Google Tasks.
 * @return Nothing.
 */
STATIC gboolean
find_missing_google_matches( gpointer key,
							 gpointer google_task_ptr, gpointer matches_ptr )
{
	struct merged_tasks_t *matches     = matches_ptr;
	gtask_t               *google_task = google_task_ptr;

	/* Determine whether the Google Task is matched against an iCalendar
	   todo. */
	if( ( g_tree_lookup( matches->merged_tasks, google_task->id ) == NULL ) &&
		( g_tree_lookup( matches->merged_tasks,
						 google_task->x_google_task_id ) == NULL ) )
	{
		/* No match; determine whether the task is in the problems list. */
		if( google_task_is_in_problems_list( matches->problems, google_task )
			== FALSE )
		{
			/* Add the Google task to the unmatched tasks list. */
			matches->unmatched_google_tasks =
				g_slist_append( matches->unmatched_google_tasks, google_task );
		}
	}

	/* Always return false to indicate that the entire tree must be
	   traversed. */
	return( FALSE );
}



/**
 * Search the Google Tasks for a task that matches the supplied iCalendar todo
 * entry and merge them into one task, if possible.  The function searches the
 * Google Tasks for an ID that matches the iCalendar todo ID and attempts to
 * merge them into a single task.  The ID of the iCalendar todo is added to a
 * sync problems list if the merge is unsuccessful, or to a list of unmatched
 * todo entries if no Google Task match was found.
 * @param key [in] Unused.
 * @param ical_todo_ptr [in] Pointer to the iCalendar todo entry.
 * @param search_ptr [in] Pointer to a structure containing a merged entries
 *        list, an unmatched entries list, and a problem ID list.
 * @return Nothing.
 */
STATIC gboolean
merge_one_icalendar_match( gpointer key,
						   gpointer ical_todo_ptr, gpointer search_ptr )
{
	const icalendar_t          *ical_todo         = ical_todo_ptr;
	struct match_pair_search_t *match_pair_search = search_ptr;
	const gtask_t              *google_task;
	unified_task_t             *merged_task;
	gboolean                   stop_foreach;

	/* Find the Google task that has an ID or an x-google-task-id that is
	   identical to the UID of the iCalendar todo. */
	google_task = g_tree_lookup( match_pair_search->all_google_tasks,
								 ical_todo->id );
	/* Merge if a corresponding Google Task was found. */
	if( google_task != NULL )
	{
		merged_task = merge_matching_tasks( (const icalendar_t*) ical_todo,
											(const gtask_t*) google_task );
		/* If the tasks could not be merged, add the ID to the problems
		   list. */
		if( merged_task == NULL )
		{
			match_pair_search->merged_tasks.problems = g_slist_append(
				match_pair_search->merged_tasks.problems, ical_todo->id );
		}
		stop_foreach = TRUE;
	}
	/* Add the iCalendar todo to the unmatched list if no corresponding
	   Google Task was found. */
	else
	{
		match_pair_search->merged_tasks.unmatched_icalendar_todos =
			g_slist_append(
				match_pair_search->merged_tasks.unmatched_icalendar_todos,
				(icalendar_t*) ical_todo );
		stop_foreach = FALSE;
	}

	return( stop_foreach );
}



/**
 * Create a list of all tasks merged into one tree.
 * @param icalendar_todos [in] Collection of iCalendar todo entries.
 * @param google_tasks [in] Collection of Google Tasks.
 * @return List of matching Google Task and iCal todo combos, followed by
 *         non-matching entries.
 */
struct match_pair_search_t*
merge_tasks( GTree *icalendar_todos, GTree *google_tasks )
{
	struct match_pair_search_t *match_pair_search;
	match_pair_search = g_new0( struct match_pair_search_t, 1 );

	/* Attempt to find a match for each entry in the icalendar tree.  This
	   function finds all matches and adds the iCalendar entries without a
	   match to the iCalendar-unmatched list.  Problem matches are added
	   to the problems list. */
    match_pair_search->all_google_tasks = google_tasks;
	g_tree_foreach( icalendar_todos, merge_one_icalendar_match,
					 match_pair_search );
	/* Next, add the Google Tasks that do not have a match. */
	g_tree_foreach( google_tasks, find_missing_google_matches,
					&match_pair_search->merged_tasks );

	/* The match_pair_search structure now contains:
	     - Merged tasks,
		 - Unmatched iCalendar todos,
		 - Unmatched Google Tasks, and
		 - Problem IDs.	*/

	/* Convert the unmatched lists to unified tasks in the merged tasks tree. */



	return( match_pair_search );
}

