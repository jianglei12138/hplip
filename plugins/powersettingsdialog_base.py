# -*- coding: utf-8 -*-

# Form implementation generated from reading ui file '/home/dwelch/linux-imaging-and-printing/src/plugins/powersettingsdialog_base.ui'
#
# Created: Wed May 11 12:45:25 2005
#      by: The PyQt User Interface Compiler (pyuic) 3.14.1
#
# WARNING! All changes made in this file will be lost!


import sys
from qt import *


class PowerSettingsDialog_base(QDialog):
    def __init__(self,parent = None,name = None,modal = 0,fl = 0):
        QDialog.__init__(self,parent,name,modal,fl)

        if not name:
            self.setName("PowerSettingsDialog_base")


        PowerSettingsDialog_baseLayout = QGridLayout(self,1,1,11,6,"PowerSettingsDialog_baseLayout")

        self.line1 = QFrame(self,"line1")
        self.line1.setFrameShape(QFrame.HLine)
        self.line1.setFrameShadow(QFrame.Sunken)
        self.line1.setFrameShape(QFrame.HLine)

        PowerSettingsDialog_baseLayout.addMultiCellWidget(self.line1,1,1,0,2)

        self.textLabel1 = QLabel(self,"textLabel1")

        PowerSettingsDialog_baseLayout.addMultiCellWidget(self.textLabel1,0,0,0,2)

        self.pushButton1 = QPushButton(self,"pushButton1")

        PowerSettingsDialog_baseLayout.addWidget(self.pushButton1,4,2)
        spacer1 = QSpacerItem(361,20,QSizePolicy.Expanding,QSizePolicy.Minimum)
        PowerSettingsDialog_baseLayout.addItem(spacer1,4,0)

        self.pushButton2 = QPushButton(self,"pushButton2")

        PowerSettingsDialog_baseLayout.addWidget(self.pushButton2,4,1)
        spacer2 = QSpacerItem(20,171,QSizePolicy.Minimum,QSizePolicy.Expanding)
        PowerSettingsDialog_baseLayout.addItem(spacer2,3,0)

        self.power_setting = QButtonGroup(self,"power_setting")
        self.power_setting.setColumnLayout(0,Qt.Vertical)
        self.power_setting.layout().setSpacing(6)
        self.power_setting.layout().setMargin(11)
        power_settingLayout = QGridLayout(self.power_setting.layout())
        power_settingLayout.setAlignment(Qt.AlignTop)
        spacer4 = QSpacerItem(121,20,QSizePolicy.Expanding,QSizePolicy.Minimum)
        power_settingLayout.addItem(spacer4,1,2)

        self.power_setting_combo = QComboBox(0,self.power_setting,"power_setting_combo")
        self.power_setting_combo.setEnabled(0)

        power_settingLayout.addWidget(self.power_setting_combo,1,1)

        self.radioButton2 = QRadioButton(self.power_setting,"radioButton2")
        self.power_setting.insert( self.radioButton2,1)

        power_settingLayout.addWidget(self.radioButton2,1,0)

        self.radioButton1 = QRadioButton(self.power_setting,"radioButton1")
        self.radioButton1.setChecked(1)
        self.power_setting.insert( self.radioButton1,0)

        power_settingLayout.addMultiCellWidget(self.radioButton1,0,0,0,2)

        PowerSettingsDialog_baseLayout.addMultiCellWidget(self.power_setting,2,2,0,2)

        self.languageChange()

        self.resize(QSize(636,463).expandedTo(self.minimumSizeHint()))
        self.clearWState(Qt.WState_Polished)

        self.connect(self.pushButton2,SIGNAL("clicked()"),self.reject)
        self.connect(self.pushButton1,SIGNAL("clicked()"),self.accept)
        self.connect(self.radioButton1,SIGNAL("toggled(bool)"),self.power_setting_combo.setDisabled)
        self.connect(self.power_setting,SIGNAL("clicked(int)"),self.power_setting_clicked)


    def languageChange(self):
        self.setCaption(self.__tr("HP Device Manager - Battery Mode Power Settings"))
        self.textLabel1.setText(self.__tr("<b>Configure the power off settings when operating on battery power</b>"))
        self.pushButton1.setText(self.__tr("OK"))
        self.pushButton2.setText(self.__tr("Cancel"))
        self.power_setting.setTitle(self.__tr("Power Settings"))
        self.radioButton2.setText(self.__tr("Automatically turn off printer after:"))
        self.radioButton1.setText(self.__tr("Always leave printer on"))


    def power_setting_clicked(self,a0):
        print("PowerSettingsDialog_base.power_setting_clicked(int): Not implemented yet")

    def __tr(self,s,c = None):
        return qApp.translate("PowerSettingsDialog_base",s,c)

if __name__ == "__main__":
    a = QApplication(sys.argv)
    QObject.connect(a,SIGNAL("lastWindowClosed()"),a,SLOT("quit()"))
    w = PowerSettingsDialog_base()
    a.setMainWidget(w)
    w.show()
    a.exec_loop()
