#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# (c) Copyright 2003-2015 HP Development Company, L.P.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
#
# Author: Don Welch
#
from __future__ import print_function
__version__ = '5.1'
__title__ = 'HPLIP Installer'
__mod__ = 'hplip-install'
__doc__ = "Installer for HPLIP tarball (called automatically after invoking the .run file)."


# Std Lib
import getopt
import os
import os.path
import sys
import time
import re

# Local
from base.g import *
from base import utils, os_utils


USAGE = [(__doc__, "", "name", True),
         ("Usage: sh %s [OPTIONS]" % __mod__, "", "summary", True),
         utils.USAGE_SPACE,
         utils.USAGE_SPACE,
         utils.USAGE_OPTIONS,
         ("Automatic mode (chooses the most common options):", "-a or --auto", "option", False),
         ("Dependency package installation retries:", "-r <retries> or --retries=<retries> (default is 3)", "option", False),
         ("Assume network connection present:", "-n or --network", "option", False),
         utils.USAGE_LOGGING1, utils.USAGE_LOGGING2, utils.USAGE_LOGGING3,
         utils.USAGE_HELP,
         utils.USAGE_SPACE,
         utils.USAGE_SPACE,
         ("[OPTIONS] (FOR TESTING ONLY/ADVANCED)", "", "header", False),
         ("Force install of all dependencies:", "-x", "option", False),
         ("Force unknown distro mode:", "-d", "option", False),
         ("Force installation of Qt4 support:", "--qt4 (same as --enable=qt4)", "option", False),
         ("Force disable Qt4 support:", "--no-qt4 (same as --disable=qt4", "option", False),
         #("Force installation of Qt3 support:", "--qt3 (same as --enable=qt3)", "option", False),
         #("Force disable Qt3 support:", "--no-qt3 (same as --disable=qt3", "option", False),
         ("Force installation of PolicyKit support:", "--policykit (same as --enable=policykit)", "option", False),
         ("Force disable PolicyKit support:", "--no-policykit (same as --disable=policykit)", "option", False),
         ("Force configure enable/disable flag:", "--enable=<flag> or --disable=<flag>, where <flag> is 'fax-build', 'qt4', 'pp-build', etc. See ./configure --help for more info.", "option", False),
        ]

def usage(typ='text'):
    if typ == 'text':
        utils.log_title(__title__, __version__)

    utils.format_text(USAGE, typ, __title__, __mod__, __version__)
    sys.exit(0)


log.set_module(__mod__)

log.debug("euid = %d" % os.geteuid())
mode = INTERACTIVE_MODE
auto = False
test_depends = False
test_unknown = False
language = None
assume_network = False
max_retries = 3
restricted_override = False
enable = []
disable = []

if((re.search(' ',os.getcwd()))!= None):
        log.info("Current hplip source directory path has space character in it. Please update path by removing space characters. Example: Change %s.run to %s.run" % (os.getcwd(),(os.getcwd()).replace(' ','')))
        cmd = "rm -r ../%s" % (os.getcwd()).rsplit('/').pop()
        os_utils.execute(cmd)
        sys.exit(0)

try:
    opts, args = getopt.getopt(sys.argv[1:], 'hl:giatxdq:nr:b',
        ['help', 'help-rest', 'help-man', 'help-desc', 'gui', 'lang=',
        'logging=', 'interactive', 'auto', 'text', 'qt4',
        'network', 'retries=', 'enable=', 'disable=',
        'no-qt4', 'policykit', 'no-policykit', 'debug'])

except getopt.GetoptError as e:
    log.error(e.msg)
    usage()
    sys.exit(1)

if os.getenv("HPLIP_DEBUG"):
    log.set_level('debug')

for o, a in opts:
    if o in ('-h', '--help'):
        usage()

    elif o == '--help-rest':
        usage('rest')

    elif o == '--help-man':
        usage('man')

    elif o in ('-q', '--lang'):
        language = a.lower()

    elif o == '--help-desc':
        print(__doc__, end=' ')
        sys.exit(0)

    elif o in ('-l', '--logging'):
        log_level = a.lower().strip()
        if not log.set_level(log_level):
            usage()

    elif o in ('-g', '--debug'):
        log.set_level('debug')

    elif o in ('-i', '--interactive', '--text', '-t'):
        mode = INTERACTIVE_MODE

    elif o in ('-a', '--auto'):
        auto = True

    elif o == '-x':
        log.warn("Install all depends (-x) is for TESTING ONLY")
        test_depends = True

    elif o == '-d':
        log.warn("Unknown distro (-d) is for TESTING ONLY")
        test_unknown = True

    elif o in ('-n', '--network'):
        assume_network = True

    elif o in ('-r', '--retries'):
        try:
            max_retries = int(a)
        except ValueError:
            log.error("Invalid value for retries. Set to default of 3.")
            max_retries = 3

    elif o == '-b':
        restricted_override = True

    elif o == '--qt4':
        if 'qt4' not in enable and 'qt4' not in disable:
            enable.append('qt4')
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)

    elif o == '--no-qt4':
        if 'qt4' not in disable and 'qt4' not in enable:
            disable.append('qt4')
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)

    elif o == '--policykit':
        if 'policykit' not in enable and 'policykit' not in disable:
            enable.append('policykit')
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)

    elif o == '--no-policykit':
        if 'policykit' not in disable and 'policykit' not in enable:
            disable.append('policykit')
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)

    elif o == '--enable':
        if a not in enable and a not in disable:
            enable.append(a)
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)

    elif o == '--disable':
        if a not in enable and a not in disable:
            disable.append(a)
        else:
            log.error("Duplicate configuration flag: %s" % a)
            sys.exit(1)



if os.getuid() == 0:
    log.warn("hplip-install should not be run as root.")

log_file = os.path.normpath('./hplip-install_%s.log' % time.strftime("%a-%d-%b-%Y_%H:%M:%S"))

if os.path.exists(log_file):
    os.remove(log_file)

log.set_logfile(log_file)
log.set_where(log.LOG_TO_CONSOLE_AND_FILE)

log.debug("Log file=%s" % log_file)

ac_init_pat = re.compile(r"""AC_INIT\(\[(.*?)\], *\[(.*?)\], *\[(.*?)\], *\[(.*?)\] *\)""", re.IGNORECASE)
try:
    config_in = open('./configure.in', 'r')
except IOError:
    prop.version = 'x.x.x'
else:
    for c in config_in:
        if c.startswith("AC_INIT"):
            match_obj = ac_init_pat.search(c)
            prop.version = match_obj.group(2)
            break

    config_in.close()

utils.log_title(__title__, __version__, True)

log.info("Installer log saved in: %s" % log.bold(log_file))
log.info("")


try:
    from installer import text_install
    log.debug("Starting text installer...")
    text_install.start(language, auto, test_depends, test_unknown, assume_network, max_retries, enable, disable)
except KeyboardInterrupt:
    log.error("User exit")

