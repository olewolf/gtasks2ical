/**
 * \file icalendar.c
 * \brief Read and write iCalendar todo entries.
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
#include <string.h>
#include <libical/icalfileset.h>
#include "gtasks2ical.h"
#include "icalendar.h"
#include "merge.h"


#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"


/* Global configuration data. */
extern struct configuration_t global_config;



/**
 * Add all VTODO components in a file to a list of iCal todo entries.
 * @param ical_todos [in] Current list of iCal todo entries.
 * @param filename [in] Path of the iCal file.
 * @return List of iCal todo entries.
 */
GSList*
read_vtodo_from_ical_file( GSList *ical_todos, const gchar *filename )
{
	icalset       *ical_set;
	icalcomponent *component;
	icalcomponent *c;

	/* Create an iCalendar set based on contents of the specified file. */
	ical_set = icalfileset_new( filename );

	/* Construct a list of iCalendar todo entries based on the VTODO components
	   in the iCalendar set. */
	for( component = icalfileset_get_first_component( ical_set );
		 component != NULL;
		 component = icalfileset_get_next_component( ical_set ) )
	{
		/* If the iCalendar component has a VTODO subcomponent then copy
		   the component into our iCalendar todo list. */
		for( c = icalcomponent_get_first_component( component,
													ICAL_VTODO_COMPONENT );
			 c != NULL;
			 c = icalcomponent_get_next_component( component,
												   ICAL_VTODO_COMPONENT ) )
		{
			{
				/* Add the component to the iCalendar todo list. */
				ical_todos = g_slist_append( ical_todos, c );
			}
		}

		icalcomponent_free( component );
	}

	icalfileset_free( ical_set );

	return( ical_todos );
}
