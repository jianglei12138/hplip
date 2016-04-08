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
# Author: Don Welch, Amarnath Chitumalla
#

# Std Lib
import os
import sys
import getpass
import signal
import re

# Local
from base.g import * 
from base import utils, tui, os_utils
from base.sixext import PY3
from .core_install import *
from installer import pluginhandler

# Workaround due to incomplete Python3 support in Linux distros.
PIP_PACKAGE_SEARCH = "PKG_FROM_PIP:"
Fedora_Py3 = False
xsane_reg = re.compile(r'\b({0})\b'.format("xsane"))

def progress_callback(cmd="", desc="Working..."):
    if cmd:
        log.info("%s (%s)" % (cmd, desc))
    else:
        log.info(desc)


def option_question_callback(opt, desc, default='y'):
    ok, ans = tui.enter_yes_no("Do you wish to enable '%s'" % desc, default)
    if not ok: sys.exit(0)
    return ans



def start(language, auto=True, test_depends=False,
          test_unknown=False, assume_network=False,
          max_retries=3, enable=None, disable=None):
    try:
        core =  CoreInstall(MODE_INSTALLER, INTERACTIVE_MODE)
        current_version = prop.installed_version_int
        log.debug("Currently installed version: 0x%06x" % current_version)
        core.enable = enable
        core.disable = disable

        if services.running_as_root():
            log.error("You are running the installer as root. It is highly recommended that you run the installer as")
            log.error("a regular (non-root) user. Do you still wish to continue?")

            ok, ans = tui.enter_yes_no(log.bold("Continue with installation"), 'n')
            if not ans or not ok:
                sys.exit(1)

        if auto:
            log.note("Running in automatic mode. The most common options will be selected.")

        log.info("")
        log.note("Defaults for each question are maked with a '*'. Press <enter> to accept the default.")
        core.init()
        vrs =core.get_distro_data('versions_list')
        Is_Manual_Distro = False
        distro_alternate_version=None
        if core.distro_version not in vrs and len(vrs):
            distro_alternate_version= vrs[len(vrs)-1]
            if core.is_auto_installer_support(distro_alternate_version):
                log.error("%s-%s version is not supported, so all dependencies may not be installed. However trying to install using %s-%s version packages." \
                                                   %(core.distro_name, core.distro_version, core.distro_name, distro_alternate_version))
                ok, choice = tui.enter_choice("\nPress 'y' to continue auto installation. Press 'n' to quit auto instalation(y=yes, n=no*): ",['y','n'],'n')
                if not ok or choice =='n':
                    log.info("Installation exit")
                    sys.exit()
            else:
                # Even previous distro is not supported
                Is_Manual_Distro = True
        elif not core.is_auto_installer_support():
              # This distro is not supported
            Is_Manual_Distro = True


        if Is_Manual_Distro:
            log.error("Auto installation is not supported for '%s' distro so all dependencies may not be installed. \nPlease install manually as mentioned in 'http://hplipopensource.com/hplip-web/install/manual/index.html' web-site"% core.distro_name)
            ok, choice = tui.enter_choice("\nPress 'y' to continue auto installation. Press 'n' to quit auto instalation(y=yes, n=no*): ",['y','n'],'n')
            if not ok or choice =='n':
                log.info("Installation exit")
                sys.exit()

        if not auto:
            tui.title("INSTALLATION MODE")
            log.info("Automatic mode will install the full HPLIP solution with the most common options.")
            log.info("Custom mode allows you to choose installation options to fit specific requirements.")

            #if os.getenv('DISPLAY') and utils.find_browser() is not None:
            if 0:
                ok, choice = tui.enter_choice("\nPlease choose the installation mode (a=automatic*, c=custom, w=web installer, q=quit) : ",
                    ['a', 'c', 'w'], 'a')
            else:
                ok, choice = tui.enter_choice("\nPlease choose the installation mode (a=automatic*, c=custom, q=quit) : ",
                    ['a', 'c'], 'a')

            if not ok: sys.exit(0)

            if choice == 'a':
                auto = True

            elif choice == 'w':
                from . import web_install
                log.debug("Starting web browser installer...")
                web_install.start(language)
                return

        log.info("\nInitializing. Please wait...")
        prev_hplip_version = sys_conf.get("hplip","version","0.0.0")
        pluginObj = pluginhandler.PluginHandle()
        prev_hplip_plugin_status = pluginObj.getStatus()

        if test_unknown:
            core.distro_name = 'unknown'
            core.distro = 0
            core.distro_version = 0

        #
        # HPLIP INSTALLATION
        #
        core.selected_component = 'hplip'

        #
        # INTRODUCTION
        #

        tui.title("INTRODUCTION")
        if core.selected_component == 'hplip':
            log.info("This installer will install HPLIP version %s on your computer." % core.version_public)

        log.info("Please close any running package management systems now (YaST, Adept, Synaptic, Up2date, etc).")


        # For testing, mark all dependencies missing
        if test_depends:
            for d in core.have_dependencies:
                core.have_dependencies[d] = False

        num_req_missing = core.count_num_required_missing_dependencies()
        num_opt_missing = core.count_num_optional_missing_dependencies()


        #
        # CONFIRM AND SELECT DISTRO NAME AND VERSION
        #

        tui.title("DISTRO/OS CONFIRMATION")

        if core.distro_known():
            log.info("Distro appears to be %s %s.\n" % (core.get_distro_data('display_name', '(unknown)'), core.distro_version))

        log.debug("Distro = %s Distro Name = %s Display Name= %s Version = %s Supported = %s" % \
            (core.distro, core.distro_name, core.distros[core.distro_name]['display_name'], \
            core.distro_version, core.distro_version_supported))

        distro_ok, ok = False, True
        if core.distro_known():
            ok, distro_ok = tui.enter_yes_no('Is "%s %s" your correct distro/OS and version'
                % (core.get_distro_data('display_name', '(unknown)'), core.distro_version))

        if not ok:
            sys.exit(0)

        if PY3 and core.distro_name.lower() == 'fedora' and int(core.distro_version) < 22:
             global Fedora_Py3               # Workaround due to incomplete Python3 support in Linux distros.
             Fedora_Py3 = True

        if distro_alternate_version:
            core.distro_version = distro_alternate_version

        core.distro_changed()
        if not distro_ok:
            tui.title("DISTRO/OS SELECTION")
            core.distro, core.distro_version = DISTRO_UNKNOWN, DISTRO_VER_UNKNOWN

            log.info(log.bold("\nChoose the name of the distro/OS that most closely matches your system:\n"))

            max_name = 0
            for d in core.distros_index:
                dd = core.distros[core.distros_index[d]]
                if dd['display']:
                    max_name = max(max_name, len(dd['display_name']))

            formatter = utils.TextFormatter(
                    (
                        {'width': 4},
                        {'width': max_name, 'margin': 2},
                    )
                )

            log.info(formatter.compose(("Num.", "Distro/OS Name")))
            log.info(formatter.compose(('-'*4, '-'*(max_name))))

            d_temp = {}
            x = 0
            for d in core.distros_index:
                dd = core.distros[core.distros_index[d]]

                if dd['display']:
                    d_temp[x] = d
                    log.info(formatter.compose((str(x), dd['display_name'])))
                    x += 1

            ok, y = tui.enter_range("\nEnter number 0...%d (q=quit) ?" % (x-1), 0, x-1)
            if not ok: sys.exit(0)

            core.distro = d_temp[y]
            core.distro_name = core.distros_index[core.distro]
            distro_display_name = core.distros[core.distro_name]['display_name']
            log.debug("Distro = %s Distro Name = %s Display Name= %s" %
                (core.distro, core.distro_name, distro_display_name))

            if core.distro != DISTRO_UNKNOWN:
                versions = list(core.distros[core.distro_name]['versions'].keys())
                versions.sort(key=utils.cmp_to_key(cmp))

                log.info(log.bold('\nChoose the version of "%s" that most closely matches your system:\n' % distro_display_name))
                formatter = utils.TextFormatter(
                        (
                            {'width': 4},
                            {'width': 40, 'margin': 2},
                        )
                    )

                log.info(formatter.compose(("Num.", "Distro/OS Version")))
                log.info(formatter.compose(('-'*4, '-'*40)))

                log.info(formatter.compose(("0", "Unknown or not listed")))

                x = 1
                for ver in versions:
                    ver_info = core.distros[core.distro_name]['versions'][ver]

                    if ver_info['code_name'] and ver_info['release_date']:
                        text = ver + ' ("' + ver_info['code_name'] + '", Released ' + ver_info['release_date'] + ')'

                    elif ver_info['code_name']:
                        text = ver + ' ("' + ver_info['code_name'] + '")'

                    elif ver_info['release_date']:
                        text = ver + ' (Released ' + ver_info['release_date'] + ')'

                    else:
                        text = ver

                    if not ver_info['supported']:
                        text += " [Unsupported]"

                    log.info(formatter.compose((str(x), text)))
                    x += 1

                ok, core.distro_version_int = tui.enter_range("\nEnter number 0...%d (q=quit) ?" %
                    (x-1), 0, x-1)
                if not ok: sys.exit(0)

                if core.distro_version_int == 0:
                    core.distro_version = DISTRO_VER_UNKNOWN
                    core.distro_version_supported = False
                else:
                    core.distro_version = versions[core.distro_version_int - 1]
                    core.distro_version_supported = core.get_ver_data('supported', False)

                log.debug("Distro = %s Distro Name = %s Display Name= %s Version = %s Supported = %s" % \
                    (core.distro, core.distro_name, core.distros[core.distro_name]['display_name'], \
                    core.distro_version, core.distro_version_supported))

                core.distro_changed()

                log.info("\nDistro set to: %s %s" %
                    (core.get_distro_data('display_name', '(unknown)'), core.distro_version))


#            if core.distro == DISTRO_UNKNOWN or not core.distro_version_supported:
            if core.distro == DISTRO_UNKNOWN:
                log.error("The distribution/OS that you are running is not supported. This installer\ncannot install an unsupported distribution. Please check your distribution/OS\nand re-run this installer or perform a manual installation.")
                if num_req_missing:
                    log.error("The following REQUIRED dependencies are missing and need to be installed:")

                    for d, desc, opt in core.missing_required_dependencies():
                        log.error("Missing REQUIRED dependency: %s (%s)" % (d, desc))

                for d, desc, req, opt in core.missing_optional_dependencies():
                    if req:
                        log.warning("Missing OPTIONAL dependency: %s (%s) [Required for option '%s']" % (d, desc, opt))
                    else:
                        log.warning("Missing OPTIONAL dependency: %s (%s) [Optional for option '%s']" % (d, desc, opt))

                sys.exit(1)

        #
        # SELECT OPTIONS TO INSTALL
        #

        if not auto:
            tui.title("SELECT HPLIP OPTIONS")
            log.info("You can select which HPLIP options to enable. Some options require extra dependencies.")
            log.info("")
            num_opt_missing = core.select_options(option_question_callback)

        else:
            enable_par = False
            core.selected_options['parallel'] = False

        #
        # COLLECT SUPERUSER PASSWORD
        #
        if not services.running_as_root():
            if core.passwordObj.getAuthType() == "sudo":
                tui.title("ENTER USER PASSWORD")
            else:
                tui.title("ENTER ROOT/SUPERUSER PASSWORD")

            ok = core.check_password()
            if not ok:
                log.error("3 incorrect attempts. (or) Insufficient permissions(i.e. try with sudo user).\nExiting.")
                sys.exit(1)


        # INSTALLATION NOTES
        #

        if core.is_auto_installer_support(distro_alternate_version):
            distro_notes = core.get_distro_data('notes', '').strip()
            ver_notes = core.get_ver_data('notes', '',distro_alternate_version).strip()

            if distro_notes or ver_notes:
                tui.title("INSTALLATION NOTES")

                if distro_notes:
                    log.info(distro_notes)

                if ver_notes:
                    log.info(ver_notes)

                log.info("")

                if not tui.continue_prompt("Please read the installation notes."):
                    sys.exit(0)

        sec_package_name, is_sec_installed = core.security_package_status()

        if sec_package_name:
            tui.title("SECURITY PACKAGES")
            log.info("%s is installed. " % sec_package_name)
            log.info("%s protects the application from external intrusion attempts making the application secure" % sec_package_name )
            ok, answer = tui.enter_yes_no("\nWould you like to have this installer install the hplip specific policy/profile", default_value='')
            if not ok:
                sys.exit(0)
            elif answer:
                if sec_package_name == "SELinux":
                    core.selinux_install()
                core.security_package = sec_package_name
            else:
                pass


        #
        # PRE-INSTALL COMMANDS
        #
        tui.title("RUNNING PRE-INSTALL COMMANDS")
        if core.run_pre_install(progress_callback, distro_alternate_version): # some cmds were run...
            num_req_missing = core.count_num_required_missing_dependencies()
            num_opt_missing = core.count_num_optional_missing_dependencies()
        log.info("OK")

        #
        # REQUIRED DEPENDENCIES INSTALL
        #
        package_mgr_cmd = core.get_distro_ver_data('package_mgr_cmd')
        depends_to_install = []
        depends_to_install_using_pip = []

        if num_req_missing or num_opt_missing:
            tui.title("MISSING DEPENDENCIES")
            log.info("Following dependencies are not installed. HPLIP will not work if all REQUIRED dependencies are not installed and some of the HPLIP features will not work if OPTIONAL dependencies are not installed.")

            log.info("%-20s %-20s %-20s"%( "Package-Name", "Component", "Required/Optional"))
            for d in core.dependencies:
                if (not core.have_dependencies[d]):
                    if core.dependencies[d][0]:
                        deptype = "REQUIRED"
                    else:
                        deptype = "OPTIONAL"
                        
                    log.info("%-20s %-20s %-20s" %(d, core.dependencies[d][1][0], deptype))

            ok, ans = tui.enter_yes_no("Do you want to install these missing dependencies")
            if not ok:
                sys.exit(0)  

            if not ans and num_req_missing:
                log.error("Installation can not continue because all REQUIRED dependencies are not installed.")                
                sys.exit(0)  


        if num_req_missing:
            tui.title("INSTALL MISSING REQUIRED DEPENDENCIES")
            log.notice("Installation of dependencies requires an active internet connection.")
            
            for depend, desc, option in core.missing_required_dependencies():
                log.warning("Missing REQUIRED dependency: %s (%s)" % (depend, desc))
                if Fedora_Py3:              # Workaround due to incomplete Python3 support in Linux distros.
                      continue

                ok = False
                packages, commands = core.get_dependency_data(depend,distro_alternate_version)
                log.debug("Packages: %s" % ','.join(packages))
                log.debug("Commands: %s" % ','.join(commands))

#                if core.distro_version_supported and (packages or commands):
                if package_mgr_cmd and (packages or commands):
                    if auto:
                        answer = True
                    else:
                        ok, answer = tui.enter_yes_no("\nWould you like to have this installer install the missing dependency")
                        if not ok: sys.exit(0) ### TBD: TELL CUSTOMER THAT YOU ARE QUITTING 

                    if answer:
                        ok = True
                        log.debug("Adding '%s' to list of dependencies to install. %s" % (depend,packages))
                        #Adding package which requires python(3)-pip package.
                        for pk in packages:
                            if PIP_PACKAGE_SEARCH in pk:
                                depends_to_install_using_pip.append(pk.replace(PIP_PACKAGE_SEARCH,''))
                        
                        depends_to_install.append(depend)

                else:
                    log.warn("This installer cannot install '%s' for your distro/OS and/or version." % depend)

                if not ok:
                    log.error("Installation cannot continue without this dependency. Please manually install this dependency and re-run this installer.")
                    sys.exit(0)


            if Fedora_Py3:                 # Workaround due to incomplete Python3 support in Linux distros.
                log.info("")
                log.error("'yum' tool required for package downloads is not supported for Python 3 environments by Fedora.\nHPLIP installation failed.")
                log.info("")
                log.warn(log.bold("Manually install the required dependencies to to use HPLIP with Python 3.x on Fedora. More information is available at http://hplipopensource.com/node/369"))
                log.info("")
                sys.exit(1)


        #
        # OPTIONAL dependencies
        #
        if num_opt_missing:
            tui.title("INSTALL MISSING OPTIONAL DEPENDENCIES")
            log.notice("Installation of dependencies requires an active internet connection.")

            for depend, desc, required_for_opt, opt in core.missing_optional_dependencies():

                if required_for_opt:
                    log.warning("Missing REQUIRED dependency for option '%s': %s (%s)" % (opt, depend, desc))

                else:
                    log.warning("Missing OPTIONAL dependency for option '%s': %s (%s)" % (opt, depend, desc))

                if Fedora_Py3:          # Workaround due to incomplete Python3 support in Linux distros.
                      continue

                installed = False
                packages, commands = core.get_dependency_data(depend,distro_alternate_version)
                log.debug("Packages: %s" % ','.join(packages))
                log.debug("Commands: %s" % ','.join(commands))


#                if core.distro_version_supported and (packages or commands):
                if package_mgr_cmd and (packages or commands):
                    if auto:
                        answer = True
                    else:
                        ok, answer = tui.enter_yes_no("\nWould you like to have this installer install the missing dependency")
                        if not ok: sys.exit(0)

                    if answer:
                        log.debug("Adding '%s' to list of dependencies to install. %s" % (depend,packages))
                        #Adding package which requires python(3)-pip package.
                        for pk in packages:
                            if PIP_PACKAGE_SEARCH in pk:
                                depends_to_install_using_pip.append(pk.replace(PIP_PACKAGE_SEARCH,''))

                        depends_to_install.append(depend)

                    else:
                        log.warning("Missing dependencies may affect the proper functioning of HPLIP. Please manually install this dependency after you exit this installer.")
                        log.warning("Note: Options that have REQUIRED dependencies that are missing will be turned off.")

                        if required_for_opt:
                            log.warn("Option '%s' has been turned off." % opt)
                            core.selected_options[opt] = False
                else:
                    log.warn("This installer cannot install '%s' for your distro/OS and/or version." % depend)

                    if required_for_opt:
                        log.warn("Option '%s' has been turned off." % opt)
                        core.selected_options[opt] = False

            if Fedora_Py3:                  # Workaround due to incomplete Python3 support in Linux distros.
                log.info("")
                log.warn("'yum' tool required for package downloads is not supported for Python 3 environments by Fedora.")
                log.warn("Missing dependencies may affect the proper functioning of HPLIP")
                log.info("")
                log.notice(log.bold("Manually install the above missing dependencies if required. More information is available at http://hplipopennsource.com/node/369"))
                log.info("")


        log.debug("Dependencies to install: %s  hplip_present:%s" % (depends_to_install, core.hplip_present))

#        if core.distro_version_supported and \
#            (depends_to_install or core.hplip_present) and \
#            core.selected_component == 'hplip':

        if package_mgr_cmd and \
            (depends_to_install or core.hplip_present) and \
            core.selected_component == 'hplip':
            #
            # CHECK FOR RUNNING PACKAGE MANAGER
            #
            User_exit, Is_pkg_mgr_running = core.close_package_managers()
            if User_exit:
                sys.exit(0)
#            if Is_pkg_mgr_running:
#                log.debug("Some Package manager are still running. ")

            #
            # CHECK FOR ACTIVE NETWORK CONNECTION
            #
            if not assume_network:
                tui.title("CHECKING FOR NETWORK CONNECTION")

                if not utils.check_network_connection():
                    log.error("The network appears to be unreachable. Installation may not resolve all dependencies without access to distribution repositories.")
                    ok, choice = tui.enter_choice("Do you want to continue installation without network?. Press 'y' for YES. Press 'n' for NO (y=yes*, n=no) : ",['y', 'n'], 'y')
                    if not ok or choice == 'n':
                        log.info("Please connect network and try again")
                        sys.exit(1)
                    else:
                        log.debug("Continuing installation without network")
                else:
                    log.info("Network connection present.")

            # sec_package_name, is_sec_installed = core.security_package_status()

            # if sec_package_name:
            #     tui.title("SECURITY PACKAGES")
            #     log.info("%s is installed. " % sec_package_name)
            #     log.info("%s protects the application from external intrusion attempts making the application secure" % sec_package_name )
            #     ok, answer = tui.enter_yes_no("\nWould you like to have this installer install the hplip specific policy/profile")
            #     if not ok:
            #         sys.exit(0)
            #     elif answer:
            #         core.security_package = sec_package_name
            #     else:
            #         pass

            #
            # PRE-DEPEND
            #

            tui.title("RUNNING PRE-PACKAGE COMMANDS")
            if not Fedora_Py3:                            # Workaround due to incomplete Python3 support in Linux distros.
                core.run_pre_depend(progress_callback,distro_alternate_version)
                log.info("OK")

            #
            # INSTALL PACKAGES AND RUN COMMANDS
            #

            tui.title("DEPENDENCY AND CONFLICT RESOLUTION")

            packages = []
            commands_to_run = []
            package_mgr_cmd = core.get_distro_ver_data('package_mgr_cmd')

            # HACK!
            individual_pkgs = True
            if package_mgr_cmd.startswith('xterm'):
                individual_pkgs = False

            if package_mgr_cmd:
                log.debug("Preparing to install packages and run commands...")

                for d in depends_to_install:
                    log.debug("*** Processing dependency: %s" % d)
                    pkgs, commands = core.get_dependency_data(d,distro_alternate_version)

                    if pkgs:
                        log.debug("Package(s) '%s' will be installed to satisfy dependency '%s'." %
                            (','.join(pkgs), d))

                        packages.extend(pkgs)

                    if commands:
                        log.debug("Command(s) '%s' will be run to satisfy dependency '%s'." %
                            (','.join(commands), d))

                        commands_to_run.extend(commands)

            else:
                log.error("Invalid package manager")

            log.debug("Packages: %s" % packages)
            log.debug("Commands: %s" % commands_to_run)
            log.debug("Install individual packages: %s" % individual_pkgs)

            PY_PIP = False
            if len(depends_to_install_using_pip):    # Workaround due to incomplete Python3 support in Linux distros.
                if PY3:
                    packages_to_install = 'python3-pip'
                else:
                    packages_to_install = 'python-pip'

                cmd = utils.cat(package_mgr_cmd)
                log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
                status, output = utils.run(cmd, core.passwordObj)
                if status != 0:
                    log.error("Package install command failed with error code %d" % status)
                    log.warn("Some HPLIP functionality might not function due to missing package(s). [%s]"%depends_to_install_using_pip)
                    if PY3:          # Workaround due to incomplete Python3 support in Linux distros.
                        log.notice(log.bold("More information is available at http://hplipopensource.com/node/369"))
                        sys.exit(1)
                else:
                    PY_PIP = True


            if package_mgr_cmd and packages:
                if individual_pkgs:
                    for packages_to_install in packages:
                        if PIP_PACKAGE_SEARCH in packages_to_install:
                           continue  #This will be installed using python(3)-pip
                           
                        # Workaround due to incomplete Python3 support in Linux distros.                        
                        xsane_var = PY3 and core.get_distro_data('display_name', '(unknown)') in ["Ubuntu", "Linux Mint"]
                        retries = 0
                        while True:
                            cmd = utils.cat(package_mgr_cmd)
                            # Workaround due to incomplete Python3 support in Linux distros.
                            if xsane_var and xsane_reg.search(cmd):
                                package_mgr_cmd_xsane = "sudo apt-get install --no-install-recommends --assume-yes $packages_to_install"
                                cmd = utils.cat(package_mgr_cmd_xsane)

                            log.debug("Package manager command: %s" % cmd)

                            log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
                            status, output = utils.run(cmd, core.passwordObj)

                            if status != 0:
                                retries += 1
                                if retries < (max_retries+1):
                                    log.error("Command failed. Re-try #%d..." % retries)
                                    continue

                                log.error("Package install command failed with error code %d" % status)
                                if PY3:
                                    log.notice("Some packages may not get installed on python3 due to distro incompatibilites")
                                    log.info("")
                                    log.notice("Please check for more information at http://hplipopensource.com/node/369")
                                ok, ans = tui.enter_yes_no("Would you like to retry installing the missing package(s)")

                                if not ok:
                                    sys.exit(0)

                                if ans:
                                    continue
                                else:
                                    log.warn("Some HPLIP functionality might not function due to missing package(s).")
                                    break
                            else:
                                break

                else:
                    packages_to_install = ' '.join(packages)
                    while True:
                        cmd = utils.cat(package_mgr_cmd)
                        log.debug("Package manager command: %s" % cmd)

                        log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
                        status, output = utils.run(cmd, core.passwordObj)

                        if status != 0:
                            log.error("Package install command failed with error code %d" % status)
                            ok, ans = tui.enter_yes_no("Would you like to retry installing the missing package(s)")

                            if not ok:
                                sys.exit(0)

                            if ans:
                                continue
                            else:
                                log.warn("Some HPLIP functionality might not function due to missing package(s).")
                                break
                        else:
                            break
            if PY_PIP:
                pip_cmd = utils.find_pip()
                if pip_cmd:
                    for d in depends_to_install_using_pip:
                        cmd = "%s  install %s"%(pip_cmd,d)
                        cmd = core.passwordObj.getAuthCmd()%cmd
                        log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
                        status, output = utils.run(cmd, core.passwordObj)
                        if status != 0:
                            log.error("Package install command failed with error code %d" % status)
                            log.warn("Some HPLIP functionality might not function due to missing package(s). [%s]"%d)


            if commands_to_run:
                for cmd in commands_to_run:
                    log.debug(cmd)
                    log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
                    status, output = utils.run(cmd, core.passwordObj)

                    if status != 0:
                        log.error("Install command failed with error code %d" % status)
                        sys.exit(1)


            #
            # HPLIP REMOVE
            #
            num_req_missing = 0
            core.check_dependencies()
            for depend, desc, opt in core.missing_required_dependencies():
                log.error("A required dependency '%s (%s)' is still missing." % (depend, desc))
                num_req_missing += 1

            if num_req_missing == 0 and core.hplip_present and core.selected_component == 'hplip' and core.distro_version_supported:
                path = utils.which('hp-uninstall')
                ok, choice = tui.enter_choice("HPLIP-%s exists, this may conflict with the new one being installed.\nDo you want to ('i'= Remove and Install*, 'q'= Quit)?    :"%(prev_hplip_version),['i','q'],'i')
                if not ok or choice=='q':
                    log.error("User Exit")
                    sys.exit(0)
                elif choice == 'i':
#                    log.info("Uninstalling existing HPLIP-%s"%prev_hplip_version)
                    sts =core.uninstall(NON_INTERACTIVE_MODE)

                    if sts is False:
                        log.warn("Failed to uninstall existing HPLIP-%s. This installation will overwrite on existing HPLIP" %prev_hplip_version)
                    else:
                        log.debug("HPLIP-%s is uninstalled successfully." %prev_hplip_version)

            #
            # POST-DEPEND
            #
            tui.title("RUNNING POST-PACKAGE COMMANDS")
            core.run_post_depend(progress_callback)
            log.info("OK")


            #
            # DEPENDENCIES RE-CHECK
            #
            tui.title("RE-CHECKING DEPENDENCIES")
            core.check_dependencies()

            num_req_missing = 0
            for depend, desc, opt in core.missing_required_dependencies():
                num_req_missing += 1
                log.error("A required dependency '%s (%s)' is still missing." % (depend, desc))

            if num_req_missing:
                if num_req_missing > 1:
                    log.error("Installation cannot continue without these dependencies.")
                else:
                    log.error("Installation cannot continue without this dependency.")

                log.error("Please manually install this dependency and re-run this installer.")
                sys.exit(1)

            for depend, desc, required_for_opt, opt in core.missing_optional_dependencies():
                if required_for_opt:
                    log.warn("An optional dependency '%s (%s)' is still missing." % (depend, desc))
                    log.warn("Option '%s' has been turned off." % opt)
                    core.selected_options[opt] = False
                else:
                    log.warn("An optional dependency '%s (%s)' is still missing." % (depend, desc))
                    log.warn("Some features may not function as expected.")


            if not num_opt_missing and not num_req_missing:
                log.info("OK")

        #
        # INSTALL LOCATION
        #

        log.debug("Install location = %s" % core.install_location)


        #
        # BUILD AND INSTALL
        #

        if not auto:
            tui.title("READY TO BUILD AND INSTALL")
            if not tui.continue_prompt("Ready to perform build and install."):
                sys.exit(0)

        tui.title("PRE-BUILD COMMANDS")
        core.run_pre_build(progress_callback, distro_alternate_version)
        log.info("OK")

        tui.title("BUILD AND INSTALL")
        os.umask(0o022)
        for cmd in core.build_cmds():
            log.info("Running '%s'\nPlease wait, this may take several minutes..." % cmd)
            status, output = utils.run(cmd , core.passwordObj)

            if status != 0:
                if 'configure' in cmd:
                    log.error("Configure failed with error: %s" % (CONFIGURE_ERRORS.get(status, CONFIGURE_ERRORS[1])))
                    log.error("output = %s"%output)

                else:
                    log.error("'%s' command failed with status code %d" % (cmd, status))

                sys.exit(0)
            else:
                log.info("Command completed successfully.")

            log.info("")

        log.info("\nBuild complete.")

        #
        # POST BUILD
        #

        tui.title("POST-BUILD COMMANDS")
        core.run_post_build(progress_callback, distro_alternate_version)
        try:
            from prnt import cups
            #This call is just to update the cups PPD cache file@ /var/cache/cups/ppds.dat. If this is not called, hp-setup picks incorrect ppd 1st time for some printers.
            cups.getSystemPPDs()
        except ImportError:
            log.error("Failed to Import Cups")

        #
        # OPEN MDNS MULTICAST PORT
        #
        user_conf = UserConfig()

        if core.selected_options['network']:
            open_mdns_port = core.get_distro_ver_data('open_mdns_port',None,distro_alternate_version)
            if open_mdns_port:
                tui.title("OPEN MDNS/BONJOUR FIREWALL PORT (MULTICAST PORT 5353)")

                paragraph = "In order to setup your printer on the network using mDNS/Bonjour, it is required that your internet firewall allows connections on port 5353. If this port is blocked by the firewall, connection to network printers using mDNS/Bonjour will not be possible."

                for p in tui.format_paragraph(paragraph):
                    log.info(p)
                log.info("")

                ok, ans = tui.enter_yes_no("Do you wish to open this port on your internet firewall")
                if not ok: sys.exit(0)

                if ans:
                    services.run_open_mdns_port(core, core.passwordObj)
                else:
                    log.warn("Skipping firewall setup. If this port is blocked on your firewall when setting up network printers, use SLP discovery and device URIs with ?ip=x.x.x.x. When using hp-setup, choose 'SLP' discovery under 'Advanced'.")


        #
        # Try to close running hp-systray (3.9.2 or later)
        #

        if current_version >= 0x030902: # 3.9.2
            try:
                from dbus import SessionBus, lowlevel
            except ImportError:
                pass
            else:
                try:
                    args = ['', '', EVENT_SYSTEMTRAY_EXIT, prop.username, 0, '', '']
                    msg = lowlevel.SignalMessage('/', 'com.hplip.StatusService', 'Event')
                    msg.append(signature='ssisiss', *args)
                    tui.title("CLOSE HP_SYSTRAY")
                    log.info("Sending close message to hp-systray (if it is currently running)...")
                    SessionBus().send_message(msg)
                    time.sleep(0.5)
                except:
                    pass

        tui.title("HPLIP UPDATE NOTIFICATION")
        ok, choice = tui.enter_choice("Do you want to check for HPLIP updates?. (y=yes*, n=no) : ",['y', 'n'], 'y')
        if not ok or choice != 'y':
            user_conf.set('upgrade', 'notify_upgrade', 'false')
        else:
            user_conf.set('upgrade', 'notify_upgrade', 'true')

        user_conf.set('upgrade','last_upgraded_time',str(int(time.time())))
        user_conf.set('upgrade','pending_upgrade_time','0')

        if prev_hplip_plugin_status != pluginhandler.PLUGIN_NOT_INSTALLED:
            tui.title("HPLIP PLUGIN UPDATE NOTIFICATION")
            ok, choice = tui.enter_choice("HPLIP Plug-in's needs to be installed/updated. Do you want to update plug-in's?. (y=yes*, n=no) : ",['y', 'n'], 'y')
            if ok and choice == 'y':
                ok, choice = tui.enter_choice("Do you want to install plug-in's in GUI mode?. (u=GUI mode*, i=Interactive mode) : ",['u', 'i'], 'u')
                if ok and choice == 'u':
                    plugin_cmd = 'hp-plugin  -u'
                elif ok and choice == 'i':
                    plugin_cmd = 'hp-plugin  -i'
                else:
                    log.info(log.bold("Please install hp plugin's manually, otherwise some functionality may break"))
                if os_utils.execute(plugin_cmd) != 0:
                    log.error("hp-plugin command failed. Please run hp-plugin manually.")
            else:
                log.info(log.bold("Please install hp plugin's manually, otherwise some functionality may break"))

        if core.selected_component == 'hplip':
            tui.title("RESTART OR RE-PLUG IS REQUIRED")
            cmd = "hp-setup"
            paragraph = """If you are installing a USB connected printer, and the printer was plugged in when you started this installer, you will need to either restart your PC or unplug and re-plug in your printer (USB cable only). If you choose to restart, run this command after restarting: %s  (Note: If you are using a parallel connection, you will have to restart your PC. If you are using network/wireless, you can ignore and continue).""" % cmd

            for p in tui.format_paragraph(paragraph):
                log.info(p)
            log.info("")

            ok, choice = tui.enter_choice("Restart or re-plug in your printer (r=restart, p=re-plug in*, i=ignore/continue, q=quit) : ",
                ['r', 'p', 'i'], 'p')

            if not ok:
                tui.title("RE-STARTING HP_SYSTRAY")
                services.run_systray()
                sys.exit(0)

            if choice == 'r':
                log.note("")
                log.note("IMPORTANT! Make sure to save all work in all open applications before restarting!")

                ok, ans = tui.enter_yes_no(log.bold("Restart now"), 'n')
                if not ok:
                    tui.title("RE-STARTING HP_SYSTRAY")
                    services.run_systray()
                    sys.exit(0)
                if ans:
                    ok = services.restart(core.passwordObj)
                    if not ok:
                        log.error("Restart failed. Please restart using the system menu.")

                tui.title("RE-STARTING HP_SYSTRAY")
                services.run_systray()
                sys.exit(0)

            elif choice == 'p': # 'p'
                if not tui.continue_prompt("Please unplug and re-plugin your printer now. "):
                    tui.title("RE-STARTING HP_SYSTRAY")
                    services.run_systray()
                    sys.exit(0)


        #
        # SETUP PRINTER
        #
        if core.selected_component == 'hplip':
            tui.title("PRINTER SETUP")

            if auto:
                install_printer = True
            else:
                ok, install_printer = tui.enter_yes_no("Would you like to setup a printer now")
                if not ok:
                    tui.title("RE-STARTING HP_SYSTRAY")
                    services.run_systray()
                    sys.exit(0)

            if install_printer:
                log.info("Please make sure your printer is connected and powered on at this time.")
                ok, choice = tui.enter_choice("Do you want to setup printer in GUI mode? (u=GUI mode*, i=Interactive mode) : ",['u', 'i'], 'u')
                if ok and choice == 'u':
                    setup_cmd = 'hp-setup  -u'
                    if os_utils.execute(setup_cmd) != 0:
                        log.error("hp-setup failed. Please run hp-setup manually.")

                elif ok and choice == 'i':
                    setup_cmd = 'hp-setup  -i'
                    log.info("Running '%s' command...."%setup_cmd)
                    if os_utils.execute(setup_cmd) != 0:
                        log.error("hp-setup failed. Please run hp-setup manually.")

        tui.title("RE-STARTING HP_SYSTRAY")
        services.run_systray()
    except KeyboardInterrupt:
        log.info("")
        log.error("Aborted.")

    sys.exit(0)
