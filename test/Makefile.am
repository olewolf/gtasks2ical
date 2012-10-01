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


TESTSCRIPTS = oauth2.at

noinst_PROGRAMS = test-oauth2

test_oauth2_SOURCES = test-oauth2.c $(SHAREDTESTSOURCE) ../src/oauth2-google.c

SHAREDTESTSOURCE = dispatch.c testfunctions.h

AM_CFLAGS = -I../src -DAUTOTEST -DDEBUG -O0 -g
if MAKE_OPENSSL_TESTS
AM_CFLAGS += -DMAKE_OPENSSL_TESTS
endif

TESTSUITE = testsuite

check-local: atconfig $(TESTSUITE)
	$(SHELL) $(srcdir)/$(TESTSUITE) AUTOTEST_PATH=src:test

$(TESTSUITE): $(TESTSUITE).at $(TESTSCRIPTS) atlocal.in \
	../configure ../configure.ac \
	Makefile.am Makefile.in Makefile testfunctions.h
	autom4te -l autotest $(TESTSUITE).at -o $(TESTSUITE)

$(TESTSUITE).at : $(TESTSCRIPTS) atlocal.in ../configure ../configure.ac \
	Makefile.am Makefile.in Makefile testfunctions.h
	@echo >$@ "# This file is auto-generated. Do not edit!"; echo >>$@ "";
	@echo >>$@ "m4_define([AT_PACKAGE_STRING],    [gtasks2ical Google Tasks to iCalendar Conversion])";
	@echo >>$@ "m4_define([AT_PACKAGE_BUGREPORT], [wolf@blazingangles.com])";
	@echo >>$@ "AT_INIT([Regression Tests])";
	@for f in $(TESTSCRIPTS); do echo >>$@ "m4_include([$$f])"; done

CLEANFILES = $(noinst_PROGRAMS) $(TESTSUITE).at

clean-local:
	if [ -f $(TESTSUITE) ]; then ./$(TESTSUITE) --clean; fi

distclean-local:
	rm -f atconfig atlocal
	rm -f $(TESTSUITE)

EXTRA_DIST = $(TESTSCRIPTS) $(TESTSUITE).at atlocal.in
