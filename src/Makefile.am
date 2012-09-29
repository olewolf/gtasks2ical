# Copyright (C) 2012 Ole Wolf <wolf@blazingangles.com>
#
# This file is part of gtasks2ical.
# 
# gtasks2ical is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

#AM_CFLAGS = -O2 -g -DSYSCONFDIR=${sysconfdir}
AM_CFLAGS = -O0 -g -DSYSCONFDIR=${sysconfdir}

bin_PROGRAMS = gtasks2ical

HDR = config.h gtasks2ical.h oauth2.h

gtasks2ical_SOURCES = $(HDR) gtasks2ical.c initializeconfig.c oauth2.c


