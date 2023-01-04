#************************************************************************
#       Copyright (c) 2002,2006 Harris Corporation
#               All Rights Reserved
#
#       THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF HARRIS CORPORATION
#
#       The copyright notice above does not evidence any
#       actual or intended publication of such source code.
#
#************************************************************************/

# place debugging scripts in this file, or if you have a lot of scripts, start
# a new file.

proc trace_routing {} {
	# exclude all selectable traces
	for {set ii 1} {$ii < 32} {incr ii } {
		tr exclude $ii
	}

	# just include routing trace
	tr include 29

	# start the process
	tr reset
	tr trigger
}
