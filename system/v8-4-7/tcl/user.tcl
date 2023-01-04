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

proc addgrp { group } {
	addgroup $group 4 3 0 0
	saveconfig umconfig.txt
	puts "added group $group"
}

proc addusr { user password group } {
	adduser $user $password $group 0 0
	saveconfig umconfig.txt
	puts "added user $user"
}

proc delgrp { group } {
	deletegroup $group
	saveconfig umconfig.txt
        puts "deleted group $group"
}

proc delusr { user } {
	deleteuser $user
	saveconfig umconfig.txt
        puts "deleted user $user"
}

proc showgrp {} {
	getgroup
}

proc showusr {} {
	getuser
}
