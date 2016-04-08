#!/usr/bin/python
# -*- coding: utf-8 -*-
#
# (c) Copyright 2011-2015 HP Development Company, L.P.
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
# Author: Amarnath Chitumalla
#
from __future__ import print_function
__version__ = '1.0'
__title__ = 'HPLIP upgrade latest version'
__mod__ = 'hp-upgrade'
__doc__ = "HPLIP installer to upgrade to latest version."

# Std Lib
import getopt, os, sys, re, time, datetime

# Local
from base.g import *
from base.strings import *
from base import utils, tui, module, os_utils, services, validation
from installer.core_install import *
from base.sixext.moves import input


USAGE = [(__doc__, "", "name", True),
         ("Usage: %s [OPTIONS]" % __mod__, "", "summary", True),
         utils.USAGE_SPACE,
         utils.USAGE_MODE,
         ("Run in interactive mode:", "-i or --interactive (Default)", "option", False),
#         ("Run in graphical UI mode:", "-u or --gui (future use)", "option", False),
         utils.USAGE_SPACE,
         utils.USAGE_OPTIONS,
         utils.USAGE_HELP,
         utils.USAGE_LOGGING1, utils.USAGE_LOGGING2, utils.USAGE_LOGGING3,
         ("Check for update and notify:","--notify","option",False),
         ("Check only available version:","--check","option",False),
#         ("Non-interactive mode:","-n(Without asking permissions)(future use)","option",False),
         ("Specify the path to the .run file on local system:","-p<path>","option", False),
         ("Download HPLIP package location:","-d<path> (default location ~/Downloads)","option", False),
         ("Override existing HPLIP installation even if latest vesrion is installed:","-o","option",False),
#         ("Take options from the file instead of command line:","-f<file> (future use)","option",False)
        ]

mode = INTERACTIVE_MODE
EXISTING_PACKAGE_PATH=None
PATH_TO_DOWNLOAD_INSTALLER=os.path.expanduser('~/Downloads')
FORCE_INSTALL=False
CHECKING_ONLY=False
NOTIFY=False
HPLIP_VERSION_INFO_SOURCEFORGE_SITE ="http://hplip.sourceforge.net/hplip_web.conf"
HPLIP_WEB_SITE ="http://hplipopensource.com/hplip-web/index.html"
HPLIP_PACKAGE_SITE = "http://sourceforge.net/projects/hplip/files/hplip"
IS_QUIET_MODE = False
DONOT_CLOSE_TERMINAL = False
CURRENT_WORKING_DIR = ''

def hold_terminal():
    if DONOT_CLOSE_TERMINAL:
        log.info("\n\nPlease close this terminal manually. ")
        try:
            while 1:
                pass
        except KeyboardInterrupt:
            pass


def usage(typ='text'):
    if typ == 'text':
        utils.log_title(__title__, __version__)

    utils.format_text(USAGE, typ, __title__, __mod__, __version__)
    hold_terminal()
    sys.exit(0)

def clean_exit(code=0, waitTerminal=True):
    if not NOTIFY and not CHECKING_ONLY and not IS_QUIET_MODE:
        log.info("")
        log.info("Done.")
    change_spinner_state(True)
    mod.unlockInstance()
    hold_terminal()
    sys.exit(code)


def parse_HPLIP_version(hplip_version_file, pat):
    ver = "0.0.0"
    if not os.path.exists(hplip_version_file):
        return ver

    try:
        fp= open(hplip_version_file, 'r')
    except IOError:
        log.error("Failed to get hplip version since %s file is not found."%hplip_version_file)
        return ver
    data = fp.read()
    for line in data.splitlines():
        if pat.search(line):
            ver = pat.search(line).group(1)
            break

    log.debug("Latest HPLIP version = %s." % ver)
    return ver


def get_hplip_version_from_sourceforge():
    HPLIP_latest_ver="0.0.0"

    # get HPLIP version info from hplip_web.conf file
    sts, HPLIP_Ver_file = utils.download_from_network(HPLIP_VERSION_INFO_SOURCEFORGE_SITE)
    if sts == 0:
        hplip_version_conf = ConfigBase(HPLIP_Ver_file)
        HPLIP_latest_ver = hplip_version_conf.get("HPLIP","Latest_version","0.0.0")
        os.unlink(HPLIP_Ver_file)

    return HPLIP_latest_ver


def get_hplip_version_from_hplipopensource():
    HPLIP_latest_ver="0.0.0"
    pat = re.compile(r"""The current version of the HPLIP solution is version (\d{1,}\.\d{1,}\.\d{1,}[a-z]{0,})\. \(.*""")
    sts, HPLIP_Ver_file = utils.download_from_network(HPLIP_WEB_SITE)
    if sts == 0:
        HPLIP_latest_ver = parse_HPLIP_version(HPLIP_Ver_file, pat)
        os.unlink(HPLIP_Ver_file)

    return HPLIP_latest_ver


def get_latest_hplip_version():
    HPLIP_latest_ver = get_hplip_version_from_sourceforge()

    if HPLIP_latest_ver == "0.0.0":     ## if failed to connect the sourceforge site, then check HPLIP site.
        HPLIP_latest_ver = get_hplip_version_from_hplipopensource()
                           
    return HPLIP_latest_ver


def digital_signature_fail_confirmation(msg):
    log.error(log.bold(msg))
    ok,choice = tui.enter_choice("Do you want continue without Digital Signature verification (y=yes, n=no*):", ['y','n'],'n')
    if not ok or choice == 'n':
       return False
    else:
        return True


def download_hplip_installer(path_to_download, hplip_version):
    url="%s/%s/hplip-%s.run" %(HPLIP_PACKAGE_SITE, hplip_version, hplip_version)
    hplip_package = "%s/hplip-%s.run" %(path_to_download, hplip_version)

    log.info("Downloading hplip-%s.run file..... Please wait. "%hplip_version )
    sts,download_file = utils.download_from_network(url, hplip_package, True)
    log.info("")

    if not os.path.exists(hplip_package):
        log.error("Failed to download %s file."%hplip_package)
        return '',''

    log.info("Downloading hplip-%s.run.asc file..... Please wait. "%hplip_version )
    hplip_digsig =  hplip_package+".asc"
    url = url +".asc"
    sts,download_file = utils.download_from_network(url, hplip_digsig)
    log.info("")

    if not os.path.exists(hplip_digsig):
        log.error("Failed to download %s file."%hplip_package)
        return hplip_package, ''

    return hplip_package, hplip_digsig


###################### Main ###############
log.set_module(__mod__)
try:
    mod = module.Module(__mod__, __title__, __version__, __doc__, USAGE,
                    (INTERACTIVE_MODE, GUI_MODE),
                    (UI_TOOLKIT_QT3, UI_TOOLKIT_QT4), True)

    opts, device_uri, printer_name, mode, ui_toolkit, loc = \
               mod.parseStdOpts('hl:gniup:d:of:sw', ['notify','check','help', 'help-rest', 'help-man', 'help-desc', 'interactive', 'gui', 'lang=','logging=', 'debug'],
                     handle_device_printer=False)



except getopt.GetoptError as e:
    log.error(e.msg)
    usage()

if os.geteuid() == 0:
    log.error("%s %s"  %(__mod__, queryString(ERROR_RUNNING_AS_ROOT)))
    clean_exit(1)

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
        clean_exit(0,False)

    elif o in ('-l', '--logging'):
        log_level = a.lower().strip()
        if not log.set_level(log_level):
            usage()

    elif o in ('-g', '--debug'):
        log.set_level('debug')

    elif o == '-n':
        mode = NON_INTERACTIVE_MODE
        log.info("NON_INTERACTIVE mode is not yet supported.")
        usage()
        clean_exit(0,False)

    elif o == '-p':
        EXISTING_PACKAGE_PATH=a

    elif o == '-d':
        PATH_TO_DOWNLOAD_INSTALLER=a

    elif o == '-o':
        FORCE_INSTALL = True

    elif o in ('-u', '--gui'):
        log.info("GUI is not yet supported.")
        usage()
        clean_exit(0, False)
    elif o == '--check':
        CHECKING_ONLY = True
    elif o == '--notify':
        NOTIFY = True
    elif o == '-s':
        IS_QUIET_MODE = True
    elif o == '-f':
        log.info("Option from file is not yet supported")
        usage()
        clean_exit(0, False)
    elif o == '-w':
        DONOT_CLOSE_TERMINAL = True

if not NOTIFY and not CHECKING_ONLY and not IS_QUIET_MODE:
    mod.quiet= False
    mod.showTitle()

if NOTIFY or CHECKING_ONLY:
    mod.lockInstance('check',True)
else:
    mod.lockInstance('upgrade',True)

log_file = os.path.normpath('%s/hp-upgrade.log'%prop.user_dir)

if os.path.exists(log_file):
    try:
        os.remove(log_file)
    except OSError:
        pass

log.set_logfile(log_file)
log.set_where(log.LOG_TO_CONSOLE_AND_FILE)


log.debug("Upgrade log saved in: %s" % log.bold(log_file))
log.debug("")
try:
    change_spinner_state(False)
    core =  CoreInstall(MODE_CHECK)
    if not utils.check_network_connection():
        log.error("Either Internet is not working or Wget is not installed.")
        clean_exit(1)

    installed_version=sys_conf.get("hplip","version","0.0.0")
    log.debug("HPLIP previous installed version =%s." %installed_version)

    HPLIP_latest_ver = get_latest_hplip_version()

    if HPLIP_latest_ver == "0.0.0":
        log.error("Failed to get latest version of HPLIP.")
        clean_exit(1)

    user_conf.set('upgrade','latest_available_version',HPLIP_latest_ver)
    if CHECKING_ONLY is True:
        log.debug("Available HPLIP version =%s."%HPLIP_latest_ver)

    elif NOTIFY is True:
        if not utils.Is_HPLIP_older_version(installed_version, HPLIP_latest_ver):
            log.debug("Latest version of HPLIP is already installed.")

        else:
            msg = "Latest version of HPLIP-%s is available."%HPLIP_latest_ver
            if core.is_auto_installer_support():
                distro_type= 1
            else:
                distro_type= 2

            if ui_toolkit == 'qt3':
                if not utils.canEnterGUIMode():
                    log.error("%s requires GUI support. Is Qt3 Installed?.. Exiting." % __mod__)
                    clean_exit(1)

                try:
                    from qt import *
                    from ui.upgradeform import UpgradeForm
                except ImportError:
                    log.error("Unable to load Qt3 support. Is it installed? ")
                    clean_exit(1)

                # create the main application object
                app = QApplication(sys.argv)
                QObject.connect(app, SIGNAL("lastWindowClosed()"), app, SLOT("quit()"))
                dialog = UpgradeForm(None, "",0,0,distro_type, msg)
                dialog.show()

                log.debug("Starting GUI loop...")
                app.exec_loop()

            else: #qt4
                if not utils.canEnterGUIMode4():
                    log.error("%s requires GUI support . Is Qt4 installed?.. Exiting." % __mod__)
                    clean_exit(1)

                try:
                    from PyQt4.QtGui import QApplication, QMessageBox
                    from ui4.upgradedialog import UpgradeDialog
                except ImportError:
                    log.error("Unable to load Qt4 support. Is it installed?")
                    clean_exit(1)

                app = QApplication(sys.argv)
                dialog = UpgradeDialog(None, distro_type, msg)

                dialog.show()
                log.debug("Starting GUI loop...")
                app.exec_()

    else:
        if FORCE_INSTALL is False:
            if utils.Is_HPLIP_older_version(installed_version, HPLIP_latest_ver):
                if IS_QUIET_MODE:
                    log.info("Newer version of HPLIP-%s is available."%HPLIP_latest_ver)
                ok,choice = tui.enter_choice("Press 'y' to continue to upgrade HPLIP-%s (y=yes*, n=no):"%HPLIP_latest_ver, ['y','n'],'y')
                if not ok or choice == 'n':
                    log.info("Recommended to install latest version of HPLIP-%s"%HPLIP_latest_ver)
                    clean_exit(0, False)
            else:
                log.info("Latest version of HPLIP is already installed.")
                clean_exit(0,False)

        # check distro information.
        if not core.is_auto_installer_support():
            log.info("Please install HPLIP manually as mentioned in 'http://hplipopensource.com/hplip-web/install/manual/index.html' site")
            clean_exit(0)

        if not services.close_running_hp_processes():
            clean_exit(1)

        if EXISTING_PACKAGE_PATH:
            downloaded_file = "%s/hplip-%s.run"%(EXISTING_PACKAGE_PATH, HPLIP_latest_ver)
            digsig_file = "%s/hplip-%s.run.asc"%(EXISTING_PACKAGE_PATH, HPLIP_latest_ver)
            PATH_TO_DOWNLOAD_INSTALLER = EXISTING_PACKAGE_PATH
        else:
            log.debug("\n Calling download_hplip_installer(...) \n")
            log.debug("\n System Time : %s \n"%datetime.datetime.now().time().isoformat())

            if not os.path.exists(PATH_TO_DOWNLOAD_INSTALLER):
                log.error(log.bold("No such file or directory%s"%PATH_TO_DOWNLOAD_INSTALLER))
                download_path = input(log.bold("Please specify the path to download. Press 'q' to quit:"))
                if download_path == 'q':
                    log.info("User selected to quit.")
                    clean_exit(1)            
                elif not os.path.exists(download_path):
                    log.error(log.bold("Specified path does not exist. Exiting...%s\n"%download_path)) 
                    clean_exit(1)
                elif not os.access(download_path, os.R_OK | os.W_OK):
                    log.error(log.bold("Specified path do not have enough permissions Exiting...%s\n"%download_path)) 
                    clean_exit(1)          
                else:
                    PATH_TO_DOWNLOAD_INSTALLER = download_path
            downloaded_file, digsig_file = download_hplip_installer(PATH_TO_DOWNLOAD_INSTALLER, HPLIP_latest_ver)


        gpg_obj = validation.GPG_Verification()
        digsig_sts, error_str = gpg_obj.validate(downloaded_file, digsig_file)

        if digsig_sts != ERROR_SUCCESS:
            if digsig_sts in  (ERROR_UNABLE_TO_RECV_KEYS, ERROR_DIGITAL_SIGN_NOT_FOUND, ERROR_DIGITAL_SIGN_BAD):
                if not digital_signature_fail_confirmation(error_str):
                    clean_exit(1)
            else:
                log.error(error_str)
                clean_exit(1)


        CURRENT_WORKING_DIR = os.getcwd()
        os.chdir(PATH_TO_DOWNLOAD_INSTALLER)

        # Installing hplip run.
        cmd = "sh %s" %(downloaded_file)
        log.debug("Upgrading  %s" % downloaded_file)

        sts = os_utils.execute(cmd)
        os.chdir(CURRENT_WORKING_DIR)

        if sts == 0:
            log.info(log.bold("Upgrade is Completed."))
        else:
            log.info(log.bold("Upgrade Failed or Skipped. status: %s"%sts))

    change_spinner_state(True)
    mod.unlockInstance()
    hold_terminal()

except KeyboardInterrupt:
    if CURRENT_WORKING_DIR:
        os.chdir(CURRENT_WORKING_DIR)

    if not IS_QUIET_MODE:
        log.error("User exit")

    clean_exit(1)

