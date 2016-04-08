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

# OJ H4xx battery power settings


from base.g import *
from base import device, pml

def getPowerSettings(d):
    pml_result_code, value = d.getPML(pml.OID_POWER_SETTINGS)
    d.closePML()
    log.debug("Current power settings: %s" % value)
    return value

def setPowerSettings(d, value):
    log.debug("Setting power setting to %s" % value)
    pml_result_code = d.setPML(pml.OID_POWER_SETTINGS, value)
    d.closePML()
    
