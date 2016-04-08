# -*- coding: utf-8 -*-
#
# (c) Copyright 2001-2015 HP Development Company, L.P.
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

# DJ 4x0 battery power settings

# 15min = 015
# 30min = 030
# 45min = 045
# 1hr   = 060
# 2hr   = 120
# 3hr   = 180
# never = 999

from base.g import *
from base import device

def getPowerSettings(d):
    value = d.getDynamicCounter(256, False)
    log.debug("Current power settings: %s" % value)
    d.closePrint()
    return value[6:9]

def setPowerSettings(d, value):
    log.debug("Setting power setting to %s" % value)
    pcl= \
"""\x1b%%-12345X@PJL ENTER LANGUAGE=PCL3GUI\n\x1bE\x1b%%Pmech.set_battery_autooff %s;\nudw.quit;\x1b*rC\x1bE\x1b%%-12345X""" % value
    d.printData(pcl, direct=True)
    d.closePrint()
