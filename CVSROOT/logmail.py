#! /usr/bin/python
#  -*- Python -*-

"""Complicated notification for CVS checkins.


(This script has been butchered by mdoggydog@users.sourceforge.net.
 It used to be the good old "syncmail" script, but I further removed
 the stuff that generated context diffs.  Who needs to get mailed
 a diff every time a file is checked in?  If I care, I'll browse the
 repository on the web, or do a "cvs diff" myself....)




This script is used to provide email notifications of changes to the CVS
repository.  These email changes will include context diffs of the changes.
Really big diffs will be trimmed.

This script is run from a CVS loginfo file (see $CVSROOT/CVSROOT/loginfo).  To
set this up, create a loginfo entry that looks something like this:

    mymodule /path/to/this/script %%s some-email-addr@your.domain

In this example, whenever a checkin that matches `mymodule' is made, this
script is invoked, which will generate the diff containing email, and send it
to some-email-addr@your.domain.

    Note: This module used to also do repository synchronizations via
    rsync-over-ssh, but since the repository has been moved to SourceForge,
    this is no longer necessary.  The syncing functionality has been ripped
    out in the 3.0, which simplifies it considerably.  Access the 2.x versions
    to refer to this functionality.  Because of this, the script is misnamed.

It no longer makes sense to run this script from the command line.  Doing so
will only print out this usage information.

Usage:

    %(PROGRAM)s [options] <%%S> email-addr [email-addr ...]

Where options is:

    --cvsroot=<path>
    	Use <path> as the environment variable CVSROOT.  Otherwise this
    	variable must exist in the environment.

    --help
    -h
        Print this text.

    --context=#
    -C #
        Include # lines of context around lines that differ (default: 2).

    -c
        Produce a context diff (default).

    -u
        Produce a unified diff (smaller, but harder to read).

    <%%S>
        CVS %%s loginfo expansion.  When invoked by CVS, this will be a single
        string containing the directory the checkin is being made in, relative
        to $CVSROOT, followed by the list of files that are changing.  If the
        %%s in the loginfo file is %%{sVv}, context diffs for each of the
        modified files are included in any email messages that are generated.

    email-addrs
        At least one email address.

"""

import os
import sys
import string
import time
import getopt

# Notification command
MAILCMD = '/bin/mail -s "CVS: %(SUBJECT)s" %(PEOPLE)s 2>&1 > /dev/null'

# Diff trimming stuff
DIFF_HEAD_LINES = 20
DIFF_TAIL_LINES = 20
DIFF_TRUNCATE_IF_LARGER = 1000

PROGRAM = sys.argv[0]



def usage(code, msg=''):
    print __doc__ % globals()
    if msg:
        print msg
    sys.exit(code)



def blast_mail(mailcmd, filestodiff, contextlines):
    # cannot wait for child process or that will cause parent to retain cvs
    # lock for too long.  Urg!
    if not os.fork():
        # in the child
        # give up the lock you cvs thang!
        time.sleep(2)
        fp = os.popen(mailcmd, 'w')
        fp.write(sys.stdin.read())
        fp.write('\n')
        fp.close()
        # doesn't matter what code we return, it isn't waited on
        os._exit(0)



# scan args for options
def main():
    contextlines = 2
    try:
        opts, args = getopt.getopt(sys.argv[1:], 'hC:cu',
                                   ['context=', 'cvsroot=', 'help'])
    except getopt.error, msg:
        usage(1, msg)

    # parse the options
    for opt, arg in opts:
        if opt in ('-h', '--help'):
            usage(0)
        elif opt == '--cvsroot':
            os.environ['CVSROOT'] = arg
        elif opt in ('-C', '--context'):
            contextlines = int(arg)
        elif opt == '-c':
            if contextlines <= 0:
                contextlines = 2
        elif opt == '-u':
            contextlines = 0

    # What follows is the specification containing the files that were
    # modified.  The argument actually must be split, with the first component
    # containing the directory the checkin is being made in, relative to
    # $CVSROOT, followed by the list of files that are changing.
    if not args:
        usage(1, 'No CVS module specified')
    SUBJECT = args[0]
    specs = string.split(args[0])
    del args[0]

    # The remaining args should be the email addresses
    if not args:
        usage(1, 'No recipients specified')

    # Now do the mail command
    PEOPLE = string.join(args)
    mailcmd = MAILCMD % vars()

    print 'Mailing %s...' % PEOPLE
    if specs == ['-', 'Imported', 'sources']:
        return
    if specs[-3:] == ['-', 'New', 'directory']:
        del specs[-3:]
    elif len(specs) > 2:
        L = specs[:2]
        for s in specs[2:]:
            prev = L[-1]
            if string.count(prev, ",") < 2:
                L[-1] = "%s %s" % (prev, s)
            else:
                L.append(s)
        specs = L
    print 'Generating notification message...'
    blast_mail(mailcmd, specs[1:], contextlines)
    print 'Generating notification message... done.'



if __name__ == '__main__':
    main()
    sys.exit(0)
