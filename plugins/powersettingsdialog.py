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

from base.g import *
from . import powersettings
from . import powersettings2
from base import pml

from qt import *
from .powersettingsdialog_base import PowerSettingsDialog_base


class PowerSettingsDialog(PowerSettingsDialog_base): # Dyn Ctr (DJ4xx)

    def __init__(self,value,parent = None,name = None,modal = 0,fl = 0):
        PowerSettingsDialog_base.__init__(self,parent,name,modal,fl)
        self.setting = 0 # 0=never off, 1=timed off

        log.debug("Initializing plugin dialog with power setting: %s" % value)

        self.power_setting_table = {0 : (self.__tr("15 minutes"), '015'),
                                     1 : (self.__tr("30 minutes"), '030'),
                                     2 : (self.__tr("45 minutes"), '045'),
                                     3 : (self.__tr("1 hour"), '060'),
                                     4 : (self.__tr("2 hours"), '120'),
                                     5 : (self.__tr("3 hours"), '180'),
                                    }

        for x in self.power_setting_table:
            self.power_setting_combo.insertItem(self.power_setting_table[x][0], x)

        if value == '999':
            self.power_setting.setButton(0)
            #self.setting = 0
        else:
            self.power_setting.setButton(1)
            self.setting = 1

            for x in self.power_setting_table:
                if self.power_setting_table[x][1] == value:
                    self.power_setting_combo.setCurrentItem(x)


    def power_setting_clicked(self,a0):
        self.setting = a0
        log.debug("Setting (0=Always on/1=Timed off): %s" % a0)

    def getValue(self):
        return self.power_setting_table[self.power_setting_combo.currentItem()][1]

    def __tr(self,s,c = None):
        return qApp.translate("PowerSettingsDialog",s,c)


class PowerSettingsDialog2(PowerSettingsDialog_base): # PML (OJ H4xx)

    def __init__(self,value,parent = None,name = None,modal = 0,fl = 0):
        PowerSettingsDialog_base.__init__(self,parent,name,modal,fl)
        self.setting = 0 # 0=never off, 1=timed off

        log.debug("Initializing plugin dialog with power setting: %s" % value)

        self.power_setting_table = {0 : (self.__tr("15 minutes"), pml.OID_POWER_SETTINGS_15MIN),
                                     1 : (self.__tr("30 minutes"), pml.OID_POWER_SETTINGS_30MIN),
                                     2 : (self.__tr("45 minutes"), pml.OID_POWER_SETTINGS_45MIN),
                                     3 : (self.__tr("1 hour"), pml.OID_POWER_SETTINGS_1HR),
                                     4 : (self.__tr("2 hours"), pml.OID_POWER_SETTINGS_2HR),
                                     5 : (self.__tr("3 hours"), pml.OID_POWER_SETTINGS_3HR),
                                    }

        for x in self.power_setting_table:
            self.power_setting_combo.insertItem(self.power_setting_table[x][0], x)

        if value == pml.OID_POWER_SETTINGS_NEVER:
            self.power_setting.setButton(0)
        else:
            self.power_setting.setButton(1)
            self.setting = 1

            for x in self.power_setting_table:
                if self.power_setting_table[x][1] == value:
                    self.power_setting_combo.setCurrentItem(x)


    def power_setting_clicked(self,a0):
        self.setting = a0
        log.debug("Setting (0=Always on/1=Timed off): %s" % a0)

    def getValue(self):
        return self.power_setting_table[self.power_setting_combo.currentItem()][1]

    def __tr(self,s,c = None):
        return qApp.translate("PowerSettingsDialog2",s,c)


def settingsUIDynCtr(d, parent):
    value = powersettings.getPowerSettings(d)
    log.debug("Battery power settings: %s" % value)

    dlg = PowerSettingsDialog(value, parent)

    if dlg.exec_loop() == QDialog.Accepted:
        value = dlg.getValue()
        log.debug("Power setting set to %s in dialog" % value)

        if dlg.setting == 0:
            powersettings.setPowerSettings(d, '999')
        else:
            powersettings.setPowerSettings(d, value)


def settingsUIPML(d, parent):
    value = powersettings2.getPowerSettings(d)
    log.debug("Battery power settings: %s" % value)

    dlg = PowerSettingsDialog2(value, parent)

    if dlg.exec_loop() == QDialog.Accepted:
        value = dlg.getValue()
        log.debug("Power setting set to %s in dialog" % value)

        if dlg.setting == 0:
            powersettings2.setPowerSettings(d, pml.OID_POWER_SETTINGS_NEVER)
        else:
            powersettings2.setPowerSettings(d, value)


def settingsUI(d, parent=None):
    log.debug("settingsUI(%s)" % __file__)

    battery_check = d.mq['power-settings']

    if battery_check == POWER_SETTINGS_EPML:
        return settingsUIDynCtr(d, parent)

    elif battery_check == POWER_SETTINGS_PML:
        return settingsUIPML(d, parent)

