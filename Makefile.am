#
#  Makefile.am - hplip automake input file
#
#  (c) 2004-2015 Copyright HP Development Company, LP
#  Author: David Suffield, Naga Samrat Chowdary Narla, Sarbeswar Meher
INCLUDES = -Iip -Iio/hpmud -Iscan/sane -Iprnt/hpijs -Icommon/
CFLAGS+= -DCONFDIR=\"$(hplip_confdir)\"
CXXFLAGS+= -DCONFDIR=\"$(hplip_confdir)\"

hplipdir = $(datadir)/hplip
apparmordir = /etc/apparmor.d
docdir = $(hpdocdir)
htmldir = $(hphtmldir)
doc_DATA = COPYING copyright prnt/hpijs/README_LIBJPG
EXTRA_DIST = prnt/hpijs/gdevijs-krgb-1.5-gs8.61.patch prnt/hpijs/README_LIBJPG copyright

dist_noinst_SCRIPTS =
dist_noinst_DATA =
noinst_PROGRAMS =
lib_LTLIBRARIES =

include foomatic_drv.inc
include unreleased.inc
include cups_drv.inc

if DOC_BUILD
wwwsrc = doc
www0dir = $(htmldir)
dist_www0_DATA = $(wwwsrc)/index.html $(wwwsrc)/commandline.html $(wwwsrc)/copying.html $(wwwsrc)/devicemanager.html $(wwwsrc)/faxtrouble.html $(wwwsrc)/gettinghelp.html $(wwwsrc)/hpscan.html $(wwwsrc)/mainttask.html $(wwwsrc)/plugins.html $(wwwsrc)/print.html $(wwwsrc)/printing.html $(wwwsrc)/printoptions.html $(wwwsrc)/printtroubleshooting.html $(wwwsrc)/scanning.html $(wwwsrc)/scantrouble.html $(wwwsrc)/sendfax.html $(wwwsrc)/setup.html $(wwwsrc)/systray.html $(wwwsrc)/troubleshooting.html $(wwwsrc)/uninstalling.html $(wwwsrc)/upgrading.html
www3dir = $(htmldir)/styles
dist_www3_DATA = $(wwwsrc)/styles/*
www4dir = $(htmldir)/images
dist_www4_DATA = $(wwwsrc)/images/*
endif

if HPLIP_BUILD

# hp backend.
hpdir = $(cupsbackenddir)
hp_PROGRAMS = hp
hp_SOURCES = prnt/backend/hp.c
hp_LDADD = libhpmud.la $(DBUS_LIBS)
hp_LDFLAGS = $(libhpmud_la_LDFLAGS)
hp_CFLAGS = $(DBUS_CFLAGS)

# hpaio sane backend
if SCAN_BUILD
libsane_hpaiodir = $(libdir)/sane
libsane_hpaio_LTLIBRARIES = libsane-hpaio.la
libsane_hpaio_la_SOURCES = scan/sane/hpaio.c scan/sane/mfpdtf.c scan/sane/pml.c scan/sane/scl.c scan/sane/io.c scan/sane/hpaio.h \
	  scan/sane/pml.h scan/sane/saneopts.h scan/sane/io.h scan/sane/mfpdtf.h scan/sane/sane.h scan/sane/scl.h scan/sane/tables.h \
	  scan/sane/common.c scan/sane/common.h scan/sane/soap.h scan/sane/soapht.h scan/sane/sanei_debug.h scan/sane/sanei.h \
	  scan/sane/sanei_init_debug.c scan/sane/marvell.h scan/sane/marvelli.h scan/sane/marvell.c scan/sane/soapht.c scan/sane/soap.c \
        scan/sane/soaphti.h scan/sane/soapi.h scan/sane/xml.c scan/sane/xml.h scan/sane/ledm.h scan/sane/ledmi.h scan/sane/ledm.c \
        scan/sane/bb_ledm.c scan/sane/http.h scan/sane/http.c scan/sane/sclpml.c scan/sane/sclpml.h common/utils.c common/utils.h \
	scan/sane/escl.h scan/sane/escli.h scan/sane/escl.c
if DARWIN_BUILD
libsane_hpaio_la_LDFLAGS = -module -framework CoreFoundation -version-info 1:0:0
else
libsane_hpaio_la_LDFLAGS = -version-info 1:0:0
endif
# The following is a interlibrary dependency that must be compiled first.
libsane_hpaio_la_LIBADD = libhpip.la libhpmud.la libhpipp.la $(DBUS_LIBS) -lcups -ldl
#libsane_hpaio_la_CFLAGS = -DWITH_NONAMESPACES -DSOAP_DEBUG
libsane_hpaio_la_CFLAGS = $(DBUS_CFLAGS) -Iprotocol

if NETWORK_BUILD
libsane_hpaio_la_LIBADD += libhpdiscovery.la
libsane_hpaio_la_CFLAGS += -Iprotocol/discovery
endif
endif # SCAN_BUILD


# HP Service Discovery library
if NETWORK_BUILD
lib_LTLIBRARIES += libhpdiscovery.la
libhpdiscovery_la_SOURCES = protocol/discovery/mdns.c protocol/discovery/mdns.h
libhpdiscovery_la_CFLAGS  = -DCONFDIR=\"$(hplip_confdir)\"
libhpdiscovery_la_LDFLAGS = -version-info 0:1:0
libhpdiscovery_la_LIBADD  = -l$(SNMPLIB) -lcrypto



endif

# hpmud library
MUDNAME=hpmud
lib_LTLIBRARIES += libhpmud.la
libhpmud_la_CFLAGS  = -DMUDNAME=\"$(MUDNAME)\" -DCONFDIR=\"$(hplip_confdir)\" -Iprotocol
libhpmud_la_LDFLAGS = -version-info 0:6:0 -lpthread -ldl 
libhpmud_la_SOURCES = io/hpmud/hpmud.c io/hpmud/mlc.c io/hpmud/model.c io/hpmud/pml.c \
	  io/hpmud/hpmud.h io/hpmud/hpmudi.h io/hpmud/list.h io/hpmud/mlc.h io/hpmud/pml.h io/hpmud/dot4.c \
	  io/hpmud/dot4.h io/hpmud/jd.c io/hpmud/jd.h io/hpmud/pp.c io/hpmud/pp.h io/hpmud/musb.h \
	  common/utils.c common/utils.h

if LIBUSB01_BUILD
libhpmud_la_SOURCES += io/hpmud/musb_libusb01.c 
libhpmud_la_LDFLAGS += -lusb
else
libhpmud_la_SOURCES += io/hpmud/musb.c 
libhpmud_la_CFLAGS  += -I/usr/include/libusb-1.0
libhpmud_la_LDFLAGS += -lusb-1.0
endif

if NETWORK_BUILD
libhpmud_la_CFLAGS  += -Iprotocol/discovery
libhpmud_la_LDFLAGS += -l$(SNMPLIB) -lcrypto
libhpmud_la_LIBADD  =  libhpdiscovery.la
endif



# ip library
lib_LTLIBRARIES += libhpip.la
libhpip_la_LDFLAGS = -version-info 0:1:0
libhpip_la_LIBADD = -lm
libhpip_la_SOURCES = ip/xconvolve.c ip/xfax.c ip/xgrayout.c ip/xjpg_dct.c ip/xjpg_fix.c ip/xpad.c ip/xrotate.c ip/xskel.c ip/xtiff.c \
	ip/ipmain.c ip/xchgbpp.c ip/xcrop.c ip/xgamma.c ip/xjpg_dec.c ip/xjpg_huf.c ip/xpcx.c ip/xsaturation.c ip/xtable.c ip/xtonemap.c \
	ip/xbi2gray.c ip/xcolrspc.c ip/xfakemono.c ip/xgray2bi.c ip/xinvert.c ip/xjpg_enc.c ip/xmatrix.c ip/xpnm.c ip/xscale.c ip/xthumb.c ip/xyxtract.c \
	ip/hpip.h ip/ipdefs.h ip/xform.h ip/xjpg_dct.h ip/xjpg_huf.h ip/xjpg_mrk.h


#hpmud rules data dir
rulessystemdir=/usr/lib/systemd/system
dist_rulessystem_DATA =data/rules/hplip-printer@.service

# hpmud.rules
rulesdir = /etc/udev/rules.d
if UDEV_SYSFS_RULES
dist_rules_DATA = data/rules/56-hpmud_sysfs.rules
else
dist_rules_DATA = data/rules/56-hpmud.rules
endif

halpredir = /usr/share/hal/fdi/preprobe/10osvendor
dist_halpre_DATA = data/rules/20-hplip-devices.fdi

# hplip.conf
hplip_confdir = /etc/hp
hplip_conf_DATA = hplip.conf

#pstotiff filter
pstotiffdir = $(cupsfilterdir)
pstotiff_SCRIPTS = fax/filters/pstotiff

# hplip.state
hplip_statedir = /var/lib/hp
dist_hplip_state_DATA =

if FULL_BUILD

dist_hplip_SCRIPTS = hpssd.py __init__.py hpdio.py
dist_noinst_DATA += scan/sane/hpaio.desc prnt/drv/hpijs.drv.in.template
dist_noinst_SCRIPTS += dat2drv.py install.py hplip-install init-suse-firewall init-iptables-firewall

cmddir = $(hplipdir)
dist_cmd_SCRIPTS = align.py info.py print.py toolbox.py clean.py colorcal.py unload.py testpage.py makeuri.py check.py fab.py levels.py \
	sendfax.py setup.py makecopies.py probe.py timedate.py firmware.py scan.py systray.py plugin.py linefeedcal.py pqdiag.py \
	faxsetup.py devicesettings.py printsettings.py query.py pkservice.py wificonfig.py diagnose_plugin.py uninstall.py \
	upgrade.py config_usb_printer.py diagnose_queues.py logcapture.py doctor.py

homedir = $(hplipdir)
dist_home_DATA =  check-plugin.py 
dist_home_DATA +=  hplip_clean.sh

# base
basedir = $(hplipdir)/base

dist_base_DATA = base/maint.py base/codes.py base/g.py base/pml.py base/status.py  \
	base/__init__.py base/mfpdtf.py base/utils.py base/wifi.py base/LedmWifi.py \
	base/device.py base/logger.py base/slp.py base/exif.py base/strings.py base/magic.py \
	base/imagesize.py base/models.py base/validation.py base/sixext.py base/avahi.py \
	base/mdns.py base/tui.py base/dime.py base/ldif.py base/vcard.py base/module.py \
	base/pkit.py base/queues.py base/password.py base/services.py base/os_utils.py \
	base/smart_install.py base/six.py

basepexpectdir = $(hplipdir)/base/pexpect
dist_basepexpect_DATA=base/pexpect/__init__.py

# installer
installdir = $(hplipdir)/installer
dist_install_DATA = installer/__init__.py installer/dcheck.py installer/distros.dat installer/core_install.py installer/pluginhandler.py
dist_noinst_DATA += installer/text_install.py

# makecopies
copierdir = $(hplipdir)/copier
dist_copier_DATA = copier/copier.py copier/__init__.py

# fax
if FAX_BUILD
faxdir = $(hplipdir)/fax
dist_fax_DATA = fax/fax.py fax/__init__.py fax/coverpages.py fax/pmlfax.py fax/ledmfax.py fax/soapfax.py fax/ledmsoapfax.py fax/marvellfax.py \
	fax/faxdevice.py fax/filters/pstotiff fax/filters/pstotiff.convs fax/filters/pstotiff.types
fax_filtersdir = $(mimedir)
dist_fax_filters_DATA = fax/filters/pstotiff.convs fax/filters/pstotiff.types
hpfaxdir = $(cupsbackenddir)
dist_hpfax_SCRIPTS = fax/backend/hpfax.py
hpijsfaxppddir = $(hpppddir)
hpcupsfaxppddir = $(hpppddir)
if HPIJS_INSTALL
dist_hpijsfaxppd_DATA = fax/ppd/HP-Fax-hpijs.ppd.gz fax/ppd/HP-Fax2-hpijs.ppd.gz fax/ppd/HP-Fax3-hpijs.ppd.gz fax/ppd/HP-Fax4-hpijs.ppd.gz
endif # HPIJS_INSTALL
if HPCUPS_INSTALL
dist_hpcupsfaxppd_DATA = fax/ppd/HP-Fax-hpcups.ppd.gz fax/ppd/HP-Fax2-hpcups.ppd.gz fax/ppd/HP-Fax3-hpcups.ppd.gz fax/ppd/HP-Fax4-hpcups.ppd.gz
endif # HPCUPS_INSTALL
endif # FAX_BUILD

# AppArmor Changes
if APPARMOR_BUILD
apparmor_profiledir = /etc/apparmor.d
apparmor_profile_DATA = apparmor/usr.share.hplip
dist_apparmor_profile_DATA = apparmor/usr.share.hplip

apparmor_abstractiondir = /etc/apparmor.d/abstractions
apparmor_abstraction_DATA = apparmor/abstractions/hplip
dist_apparmor_abstraction_DATA = apparmor/abstractions/hplip

endif #APPARMOR_BUILD

selinuxdir = 
dist_selinux_DATA = selinux/hplip.te selinux/hplip.fc selinux/hplip.pp selinux/hplip.if


# ptest
noinst_PROGRAMS += ptest
ptest_SOURCES = pcard/ptest.c pcard/ptest.h pcard/fat.c pcard/fat.h
ptest_LDADD = libhpmud.la

# data
modelsdir = $(hplipdir)/data/models
images_16x16dir = $(hplipdir)/data/images/16x16
images_24x24dir = $(hplipdir)/data/images/24x24
images_32x32dir = $(hplipdir)/data/images/32x32
images_64x64dir = $(hplipdir)/data/images/64x64
images_128x128dir = $(hplipdir)/data/images/128x128
images_256x256dir = $(hplipdir)/data/images/256x256
images_devicesdir = $(hplipdir)/data/images/devices
images_otherdir = $(hplipdir)/data/images/other
pcldir = $(hplipdir)/data/pcl
ldldir = $(hplipdir)/data/ldl
postscriptdir = $(hplipdir)/data/ps
localzdir = $(hplipdir)/data/localization
dist_postscript_DATA = data/ps/testpage.ps.gz data/ps/clean_page.pdf.gz
dist_models_DATA = data/models/models.dat

if GUI_BUILD
dist_images_16x16_DATA = data/images/16x16/*
dist_images_24x24_DATA = data/images/24x24/*
dist_images_32x32_DATA = data/images/32x32/*
dist_images_64x64_DATA = data/images/64x64/*
dist_images_128x128_DATA = data/images/128x128/*
dist_images_256x256_DATA = data/images/256x256/*
dist_images_devices_DATA = data/images/devices/*
dist_images_other_DATA = data/images/other/*
endif

dist_pcl_DATA = data/pcl/align1_8xx.pcl.gz data/pcl/align1_9xx.pcl.gz data/pcl/align2_8xx.pcl.gz data/pcl/align3_8xx.pcl.gz data/pcl/align4_8xx.pcl.gz \
	data/pcl/align5_8xx.pcl.gz data/pcl/align2_9xx.pcl.gz data/pcl/align3_9xx.pcl.gz data/pcl/align4_450.pcl.gz data/pcl/align6_450.pcl.gz \
	data/pcl/colorcal1_450.pcl.gz data/pcl/colorcal2_450.pcl.gz data/pcl/crbcal.pcl.gz data/pcl/crcaldone.pcl.gz data/pcl/crcbcal.pcl.gz data/pcl/crccal.pcl.gz \
	data/pcl/crcpcal.pcl.gz data/pcl/crpcal.pcl.gz
dist_ldl_DATA = data/ldl/cb2pcal.ldl.gz data/ldl/cb2pcal_done.ldl.gz data/ldl/cbbcal.ldl.gz data/ldl/cbccal.ldl.gz data/ldl/cbccal_done.ldl.gz data/ldl/cbcpcal.ldl.gz \
	data/ldl/cbpcal.ldl.gz
dist_localz_DATA = data/localization/hplip_de.qm data/localization/hplip_es.qm data/localization/hplip_fr.qm data/localization/hplip_it.qm \
	data/localization/hplip_pt.qm data/localization/hplip_ru.qm data/localization/hplip_zh.qm
dist_noinst_DATA += data/localization/hplip_de.ts data/localization/hplip_es.ts data/localization/hplip_fr.ts data/localization/hplip_it.ts \
	data/localization/hplip_pt.ts data/localization/hplip_ru.ts data/localization/hplip_zh.ts

# pcard
pcarddir = $(hplipdir)/pcard
dist_pcard_DATA = pcard/__init__.py pcard/photocard.py

# pcardext
pcardextdir = $(pyexecdir)
pcardext_LTLIBRARIES = pcardext.la
pcardext_la_LDFLAGS = -module -avoid-version
pcardext_la_SOURCES = pcard/pcardext/pcardext.c pcard/fat.c
pcardext_la_CFLAGS = -I$(PYTHONINCLUDEDIR)

# prnt
prntdir = $(hplipdir)/prnt
dist_prnt_DATA = prnt/cups.py prnt/__init__.py prnt/ldl.py prnt/pcl.py prnt/colorcal.py


# IPP library
lib_LTLIBRARIES += libhpipp.la
#hpipp_LTLIBRARIES = hpipp.la
#hpipp_la_LDFLAGS = -module -avoid-version
libhpipp_la_SOURCES = protocol/hp_ipp.c protocol/hp_ipp.h protocol/hp_ipp_i.h
libhpipp_la_CFLAGS = -DCONFDIR=\"$(hplip_confdir)\"
libhpipp_la_LDFLAGS = -version-info 0:1:0
libhpipp_la_LIBADD = libhpmud.la


# cupsext
cupsextdir = $(pyexecdir)
cupsext_LTLIBRARIES = cupsext.la
cupsext_la_LDFLAGS = -module -avoid-version
cupsext_la_SOURCES =  prnt/cupsext/cupsext.c prnt/cupsext/cupsext.h
cupsext_la_CFLAGS = -I$(PYTHONINCLUDEDIR) -Iprotocol
cupsext_la_LIBADD = -lcups libhpipp.la

if SCAN_BUILD
# scan
scandir = $(hplipdir)/scan
dist_scan_DATA = scan/__init__.py scan/sane.py

# scanext
scanextdir = $(pyexecdir)
scanext_LTLIBRARIES = scanext.la
scanext_la_LDFLAGS = -module -avoid-version -lsane
scanext_la_SOURCES =  scan/scanext/scanext.c
scanext_la_CFLAGS = -I$(PYTHONINCLUDEDIR)
endif

# hpmudext
hpmudextdir = $(pyexecdir)
hpmudext_LTLIBRARIES = hpmudext.la
hpmudext_la_LDFLAGS = -module -avoid-version
hpmudext_la_SOURCES = io/mudext/hpmudext.c
hpmudext_la_CFLAGS = -I$(PYTHONINCLUDEDIR)
hpmudext_la_LIBADD = libhpmud.la

if NETWORK_BUILD
hpmudext_la_LIBADD += libhpdiscovery.la
hpmudext_la_CFLAGS += -Iprotocol/discovery
endif

if !LIBUSB01_BUILD
hpmudext_la_CFLAGS +=-I/usr/include/libusb-1.0 
endif

# ui (qt3)
if GUI_BUILD
if QT3_INSTALL
uidir = $(hplipdir)/ui
dist_ui_DATA = ui/alignform.py \
	ui/colorcalform_base.py ui/colorcalform.py ui/coloradjform_base.py ui/coloradjform.py ui/devmgr4_base.py ui/devmgr4.py \
	ui/__init__.py ui/loadpaperform_base.py ui/loadpaperform.py \
	ui/paperedgealignform_base.py ui/paperedgealignform.py ui/ui_utils.py \
	ui/settingsdialog_base.py ui/settingsdialog.py ui/aligntype6form1.py ui/aligntype6form1_base.py ui/aligntype6form2_base.py \
	ui/aligntype6form2.py ui/nodevicesform_base.py ui/nodevicesform.py ui/unloadform.py \
	ui/imagepropertiesdlg_base.py ui/imagepropertiesdlg.py ui/choosedevicedlg.py ui/chooseprinterdlg.py \
	ui/aboutdlg.py ui/aboutdlg_base.py ui/waitform.py ui/waitform_base.py ui/cleaningform_base.py ui/cleaningform.py \
	ui/cleaningform2_base.py ui/cleaningform2.py ui/colorcalform2_base.py ui/colorcalform2.py ui/colorcal4form.py \
	ui/colorcal4form_base.py ui/colorcal4form_base.ui ui/printerform.py \
	ui/faxaddrbookgroupsform_base.py ui/faxaddrbookgroupeditform_base.py ui/faxaddrbookform_base.py \
	ui/faxaddrbookform.py ui/faxaddrbookeditform_base.py ui/align10form.py ui/align10form_base.py \
	ui/faxsendjobform.py ui/faxsettingsform_base.py ui/faxsettingsform.py \
	ui/coverpageform_base.py ui/coverpageform.py ui/allowabletypesdlg_base.py ui/allowabletypesdlg.py \
	ui/makecopiesform.py ui/setupform_base.py ui/setupform.py ui/setupmanualfind_base.py \
	ui/setupmanualfind.py ui/setupsettings_base.py ui/setupsettings.py ui/scrollview.py \
	ui/scrollprintsettings.py ui/scrollprint.py ui/scrollfax.py \
	ui/scrollunload.py ui/scrollcopy.py ui/pluginform2.py ui/pluginform2_base.py ui/systemtray.py \
	ui/ui_utils.py ui/jobstoragemixin.py ui/pluginlicenseform_base.py ui/pluginlicenseform.py \
	ui/align13form_base.py ui/align13form.py ui/firmwaredialog_base.py ui/firmwaredialog.py \
	ui/deviceuricombobox.py ui/upgradeform.py ui/upgradeform_base.py

dist_noinst_DATA += ui/colorcalform2_base.ui ui/settingsdialog_base.ui ui/aligntype6form1_base.ui \
	ui/aboutdlg_base.ui ui/imagepropertiesdlg_base.ui ui/paperedgealignform_base.ui ui/aligntype6form2_base.ui \
	ui/nodevicesform_base.ui ui/cleaningform_base.ui ui/colorcalform_base.ui ui/devmgr4_base.ui ui/loadpaperform_base.ui \
	ui/cleaningform2_base.ui ui/waitform_base.ui ui/coloradjform_base.ui ui/faxaddrbookeditform_base.ui \
	ui/faxaddrbookform_base.ui ui/faxaddrbookgroupeditform_base.ui ui/faxaddrbookgroupsform_base.ui \
	ui/align10form_base.ui  ui/pluginform2_base.ui \
	plugins/powersettingsdialog_base.ui ui/faxsettingsform_base.ui ui/coverpageform_base.ui \
	ui/allowabletypesdlg_base.ui ui/setupform_base.ui ui/setupmanualfind_base.ui \
	ui/setupsettings_base.ui ui/pluginlicenseform_base.ui ui/align13form_base.ui ui/firmwaredialog_base.ui
endif
endif

#ui4 (qt4)
if GUI_BUILD
if QT4_INSTALL
ui4dir=$(hplipdir)/ui4
dist_ui4_DATA = ui4/*.py
dist_noinst_DATA += ui4/*.ui
endif
endif

# ui plugins (qt3)
if GUI_BUILD
if QT3_INSTALL
pluginsdir = $(hplipdir)/plugins
dist_plugins_DATA = plugins/dj450.py plugins/__init__.py plugins/powersettings.py plugins/powersettingsdialog.py \
	plugins/powersettingsdialog_base.py plugins/Deskjet_460.py plugins/Officejet_H470.py plugins/powersettings2.py
endif
endif

# ui plugins (qt4)
if GUI_BUILD
if QT4_INSTALL
plugins4dir = $(hplipdir)/ui4/plugins
dist_plugins4_SCRIPTS =
endif
endif

# PolicyKit
if POLICYKIT_INSTALL
policykit_dbus_etcdir = /etc/dbus-1/system.d
dist_policykit_dbus_etc_DATA = data/policykit/com.hp.hplip.conf
policykit_dbus_sharedir = /usr/share/dbus-1/system-services
dist_policykit_dbus_share_DATA = data/policykit/com.hp.hplip.service
policykit_policydir = $(policykit_dir)
dist_policykit_policy_DATA = data/policykit/com.hp.hplip.policy
endif

# hppgsz
noinst_PROGRAMS += hppgsz
hppgsz_SOURCES = prnt/hpijs/PrinterProperties.cpp prnt/hpijs/PrinterProperties.h prnt/hpijs/bug.h
hppgsz_CXXFLAGS = $(libapdk_la_CXXFLAGS)
hppgsz_CFLAGS = $(libapdk_la_CFLAGS)
hppgsz_LDADD = libapdk.la -ljpeg -ldl

if GUI_BUILD
# hplip.desktop
hplip_desktopdir = $(icondir)
hplip_desktop_DATA = hplip.desktop

# hplip-systray.desktop
hplip_systraydir = $(systraydir)
hplip_systray_DATA = hplip-systray.desktop
endif

# hpps (Finishing PS filter)
findir = $(cupsfilterdir)
dist_fin_SCRIPTS = prnt/filters/hpps


endif   # FULL_BUILD
endif	# HPLIP_BUILD

# foomatic-rip-hplip
if RIP_INSTALL
ripdir = $(cupsfilterdir)
dist_rip_SCRIPTS = prnt/hpijs/foomatic-rip-hplip
endif

# apdk
noinst_LTLIBRARIES = libapdk.la
libapdk_la_SOURCES = prnt/hpijs/models.cpp prnt/hpijs/breaks_open.cpp \
	prnt/hpijs/dj9xxvip.cpp prnt/hpijs/dj9xx.cpp prnt/hpijs/capture.cpp prnt/hpijs/colormatch.cpp prnt/hpijs/colormatcher_open.cpp \
	prnt/hpijs/compression.cpp prnt/hpijs/context2.cpp prnt/hpijs/create_so.cpp prnt/hpijs/creator.cpp \
	prnt/hpijs/dj600.cpp prnt/hpijs/dj630.cpp prnt/hpijs/dj660.cpp prnt/hpijs/dj690.cpp prnt/hpijs/dj6xx.cpp prnt/hpijs/dj8xx.cpp prnt/hpijs/dj600_maps.cpp \
	prnt/hpijs/dj660_maps.cpp prnt/hpijs/dj690_maps.cpp prnt/hpijs/dj895_maps.cpp prnt/hpijs/dj895_maps2.cpp prnt/hpijs/dj970_maps.cpp \
	prnt/hpijs/dj970_maps2.cpp prnt/hpijs/dj970_maps3.cpp prnt/hpijs/filterhpa.cpp prnt/hpijs/apollo21xx.cpp prnt/hpijs/apollo2560.cpp prnt/hpijs/apollo2xxx.cpp \
	prnt/hpijs/dj8x5.cpp prnt/hpijs/psp100.cpp prnt/hpijs/phobos_cmaps.cpp prnt/hpijs/dj350.cpp prnt/hpijs/halftoner.cpp prnt/hpijs/halftoner_open.cpp \
	prnt/hpijs/globals.cpp prnt/hpijs/header2.cpp prnt/hpijs/htmtxhi.cpp prnt/hpijs/pmselect.cpp \
	prnt/hpijs/job.cpp prnt/hpijs/printer.cpp prnt/hpijs/registry.cpp prnt/hpijs/scaler.cpp \
	prnt/hpijs/scaler_open.cpp prnt/hpijs/script.cpp prnt/hpijs/systemservices.cpp \
	prnt/hpijs/translator.cpp prnt/hpijs/version.cpp prnt/hpijs/versioncode.cpp \
	prnt/hpijs/djgenericvip.cpp prnt/hpijs/dj540.cpp prnt/hpijs/dj850.cpp prnt/hpijs/dj850_maps.cpp prnt/hpijs/dj890.cpp \
	prnt/hpijs/apollo21xx.h prnt/hpijs/apollo2560.h prnt/hpijs/apollo2xxx.h prnt/hpijs/colormatch.h prnt/hpijs/colormatcher_open.h \
	prnt/hpijs/compression.h prnt/hpijs/config.h prnt/hpijs/context.h prnt/hpijs/dj350.h prnt/hpijs/dj400.h prnt/hpijs/dj540.h \
	prnt/hpijs/dj600.h prnt/hpijs/dj630.h prnt/hpijs/dj660.h prnt/hpijs/dj690.h prnt/hpijs/dj6xx.h prnt/hpijs/dj8x5.h prnt/hpijs/dj8xx.h \
	prnt/hpijs/dj850.h prnt/hpijs/dj890.h prnt/hpijs/dj9xx.h prnt/hpijs/dj9xxvip.h \
	prnt/hpijs/ernieplatform.h prnt/hpijs/filterhpa.h prnt/hpijs/global_types.h prnt/hpijs/halftoner.h prnt/hpijs/font.h prnt/hpijs/debug.h \
	prnt/hpijs/halftoner_open.h prnt/hpijs/header.h prnt/hpijs/hpprint_c_api.h \
	prnt/hpijs/hpprintapi.h prnt/hpijs/hptypes.h prnt/hpijs/htfed.h prnt/hpijs/internal.h \
	prnt/hpijs/interp.h prnt/hpijs/interp_data_50.h prnt/hpijs/io_defs.h prnt/hpijs/job.h prnt/hpijs/models.h prnt/hpijs/modes.h prnt/hpijs/pmselect.h \
	prnt/hpijs/printer.h prnt/hpijs/psp100.h prnt/hpijs/psp470.h prnt/hpijs/resources.h prnt/hpijs/scaler_open.h prnt/hpijs/scaler_prop.h prnt/hpijs/script.h \
	prnt/hpijs/systemservices.h prnt/hpijs/unistd_.h prnt/hpijs/djgenericvip.h prnt/hpijs/ljmono.h prnt/hpijs/ljmono.cpp \
	prnt/hpijs/dj3320.cpp prnt/hpijs/dj3320.h prnt/hpijs/dj3320_cmap.cpp prnt/hpijs/dj3600.cpp prnt/hpijs/dj3600.h prnt/hpijs/dj3600_cmap.cpp prnt/hpijs/dj4100.h prnt/hpijs/djd2600.h \
	prnt/hpijs/dj4100_cmap.cpp prnt/hpijs/ldlencap.h prnt/hpijs/ljcolor.cpp prnt/hpijs/ljcolor.h prnt/hpijs/pscript.h \
	prnt/hpijs/printerproxy.cpp prnt/hpijs/printerfactory.cpp prnt/hpijs/printerproxy.h prnt/hpijs/printerfactory.h \
	prnt/hpijs/ljjetready.cpp prnt/hpijs/ljjetready.h prnt/hpijs/jdatadbf.c prnt/hpijs/jccolor.c prnt/hpijs/ljfastraster.cpp prnt/hpijs/ljfastraster.h \
	prnt/hpijs/jinclude.h prnt/hpijs/jpegint.h prnt/hpijs/dj55xx.h prnt/hpijs/hpijsfax.h prnt/hpijs/ojprokx50.h \
	prnt/hpijs/ljzjs.cpp prnt/hpijs/ljzjs.h prnt/hpijs/ljzjsmono.cpp prnt/hpijs/ljm1005.cpp prnt/hpijs/ljm1005.h \
	prnt/hpijs/ljzjsmono.h prnt/hpijs/hpjbig_wrapper.h prnt/hpijs/quickconnect.cpp prnt/hpijs/quickconnect.h prnt/hpijs/ljp1xxx.h prnt/hpijs/ljzjscolor.cpp prnt/hpijs/ljzjscolor.h \
	common/utils.c common/utils.h
libapdk_la_CXXFLAGS = -DAPDK_DJ660 -DAPDK_DJ6xx -DAPDK_DJ6xxPhoto -DAPDK_DJ8xx -DAPDK_DJ9xx -DAPDK_DJ9xxVIP -DAPDK_DJ630 \
	-DAPDK_APOLLO2XXX -DAPDK_APOLLO21XX -DAPDK_APOLLO2560 -DAPDK_DJ600 -DAPDK_DJ350 -DAPDK_DJ8x5 -DAPDK_PSP100 -DAPDK_AUTODUPLEX \
	-DAPDK_HIGH_RES_MODES -DAPDK_LJMONO -DAPDK_DJ540 -DAPDK_DJ850 -DAPDK_DJ890 -DAPDK_DJ3320 -DAPDK_LJCOLOR -DAPDK_DJGENERICVIP \
	-DAPDK_LJJETREADY  -DAPDK_LJFASTRASTER -DAPDK_BUFFER_SEND -DAPDK_LDL_COMPRESS -DAPDK_EXTENDED_MEDIASIZE \
	-DAPDK_MLC_PRINTER -DAPDK_DJ3600 -DAPDK_LJZJS_MONO -DAPDK_LJZJS_COLOR -DAPDK_LJM1005 -DAPDK_QUICKCONNECT \
                $(APDK_ENDIAN_FLAG) $(APDK_AUTO_INCLUDE_FLAG) -DAPDK_LINUX -DNDEBUG $(DBUS_CFLAGS)
libapdk_la_CFLAGS = $(libapdk_la_CXXFLAGS) -Iprnt/hpijs

if HPIJS_INSTALL
# hpijs
bin_PROGRAMS = hpijs
hpijs_SOURCES = prnt/hpijs/hpijs.cpp prnt/hpijs/ijs_server.c prnt/hpijs/ijs.c prnt/hpijs/hpijsfax.cpp prnt/hpijs/services.cpp prnt/hpijs/bug.h \
	prnt/hpijs/hpijs.h prnt/hpijs/ijs.h prnt/hpijs/ijs_server.h prnt/hpijs/services.h prnt/hpijs/ijs_client.h prnt/hpijs/hpiom.c \
	prnt/hpijs/hpiom.h common/utils.h common/utils.c

hpijs_CXXFLAGS = $(libapdk_la_CXXFLAGS)
hpijs_CFLAGS = $(libapdk_la_CFLAGS)
if HPLIP_BUILD
hpijs_LDADD = libapdk.la -ljpeg -ldl libhpip.la libhpmud.la $(DBUS_LIBS)
else
hpijs_LDADD = libapdk.la -ljpeg -ldl
endif
endif # HPIJS_INSTALL

if HPCUPS_INSTALL
# hpcups
#if NEW_HPCUPS
hpcupsdir = $(cupsfilterdir)
hpcups_PROGRAMS = hpcups
hpcups_SOURCES = prnt/hpcups/HPCupsFilter.cpp prnt/hpcups/HPCupsFilter.h prnt/hpcups/dbuscomm.cpp prnt/hpcups/dbuscomm.h prnt/hpcups/Compressor.cpp prnt/hpcups/Compressor.h \
	prnt/hpcups/Mode2.cpp prnt/hpcups/Mode2.h prnt/hpcups/Mode3.cpp prnt/hpcups/Mode3.h \
	prnt/hpcups/Mode9.cpp prnt/hpcups/Mode9.h prnt/hpcups/Mode10.cpp prnt/hpcups/Mode10.h \
	prnt/hpcups/ModeDeltaPlus.cpp prnt/hpcups/ModeDeltaPlus.h prnt/hpcups/ModeJbig.cpp prnt/hpcups/ModeJbig.h \
	prnt/hpcups/ErnieFilter.cpp prnt/hpcups/ErnieFilter.h prnt/hpcups/EncapsulatorFactory.cpp prnt/hpcups/EncapsulatorFactory.h \
	prnt/hpcups/Encapsulator.cpp prnt/hpcups/Encapsulator.h prnt/hpcups/Pcl3.cpp prnt/hpcups/Pcl3.h \
	prnt/hpcups/Pcl3Gui.cpp prnt/hpcups/Pcl3Gui.h prnt/hpcups/Pcl3Gui2.cpp prnt/hpcups/Pcl3Gui2.h \
	prnt/hpcups/LJMono.cpp prnt/hpcups/LJMono.h prnt/hpcups/LJColor.cpp prnt/hpcups/LJColor.h \
	prnt/hpcups/LJFastRaster.cpp prnt/hpcups/LJFastRaster.h prnt/hpcups/LJJetReady.cpp prnt/hpcups/LJJetReady.h \
	prnt/hpcups/LJZjStream.cpp prnt/hpcups/LJZjStream.h \
	prnt/hpcups/LJZxStream.cpp prnt/hpcups/LJZxStream.h prnt/hpcups/Job.cpp prnt/hpcups/Job.h \
	prnt/hpcups/Pipeline.cpp prnt/hpcups/Pipeline.h prnt/hpcups/Processor.cpp prnt/hpcups/Processor.h \
	prnt/hpcups/RasterSender.cpp prnt/hpcups/RasterSender.h prnt/hpcups/ColorMatcher.cpp prnt/hpcups/ColorMatcher.h \
	prnt/hpcups/Halftoner.cpp prnt/hpcups/Halftoner.h prnt/hpcups/Scaler.cpp prnt/hpcups/Scaler.h prnt/hpcups/resources.h \
	prnt/hpcups/dj400ColorMaps.cpp prnt/hpcups/dj600ColorMaps.cpp prnt/hpcups/dj970ColorMaps.cpp prnt/hpcups/dj8xxColorMaps.cpp \
	prnt/hpcups/dj4100ColorMaps.cpp \
	prnt/hpcups/BreakTables.cpp prnt/hpcups/PrinterCommands.h prnt/hpcups/Utils.cpp prnt/hpcups/Utils.h \
	prnt/hpcups/SystemServices.cpp prnt/hpcups/SystemServices.h prnt/hpcups/CommonDefinitions.h prnt/hpcups/hpjbig_wrapper.h \
	prnt/hpcups/apPrintModes.h prnt/hpcups/dj400PrintModes.h prnt/hpcups/dj540PrintModes.h prnt/hpcups/dj600PrintModes.h \
	prnt/hpcups/dj630PrintModes.h prnt/hpcups/dj690PrintModes.h prnt/hpcups/dj850PrintModes.h prnt/hpcups/dj890PrintModes.h \
	prnt/hpcups/dj895PrintModes.h prnt/hpcups/dj8x5PrintModes.h prnt/hpcups/dj970PrintModes.h prnt/hpcups/Pcl3GuiPrintModes.h \
	prnt/hpcups/Pcl3PrintModes.h prnt/hpcups/ColorMaps.h \
	prnt/hpcups/QuickConnect.cpp prnt/hpcups/QuickConnect.h \
	prnt/hpcups/ModeJpeg.cpp prnt/hpcups/ModeJpeg.h \
	prnt/hpcups/jccolor.c prnt/hpcups/jinclude.h \
	prnt/hpcups/jdatadbf.c prnt/hpcups/jinclude.h \
	prnt/hpcups/Lidil.cpp prnt/hpcups/Lidil.h \
	prnt/hpcups/LidilCompress.cpp prnt/hpcups/LidilCompress.h \
	prnt/hpcups/LidilPrintModes.h prnt/hpcups/dj3320ColorMaps.cpp \
	prnt/hpcups/dj3600ColorMaps.cpp prnt/hpcups/dj3320PrintModes.h \
	prnt/hpcups/dj4100PrintModes.h \
	prnt/hpcups/Hbpl1.cpp prnt/hpcups/Hbpl1.h prnt/hpcups/Hbpl1_Wrapper.h \
	common/utils.c common/utils.h

hpcups_CXXFLAGS = $(APDK_ENDIAN_FLAG) $(DBUS_CFLAGS)
hpcups_LDADD = -ljpeg -ldl -lcups -lcupsimage $(DBUS_LIBS)

#else
#hpcupsdir = $(cupsfilterdir)
#hpcups_PROGRAMS = hpcups
#hpcups_SOURCES = prnt/hpijs/hpcups.cpp prnt/hpijs/hpcups.h prnt/hpijs/services.cpp prnt/hpijs/services.h prnt/hpijs/hpiom.c prnt/hpijs/hpiom.h prnt/hpijs/bug.h prnt/hpijs/hpimage.cpp prnt/hpijs/hpimage.h
#hpcups_CXXFLAGS = $(libapdk_la_CXXFLAGS)
#hpcups_CFLAGS = $(libapdk_la_CFLAGS)
#if HPCUPS_ONLY_BUILD
#hpcups_LDADD = libapdk.la -ljpeg -ldl -lcups -lcupsimage
#else
#hpcups_LDADD = libapdk.la -ljpeg -ldl libhpip.la libhpmud.la $(DBUS_LIBS) -lcups -lcupsimage
#endif #HPCUPS_ONLY_BUILD
#endif # NEW_HPCUPS
if !HPCUPS_ONLY_BUILD
if FAX_BUILD
# hpcupsfax
hpcupsfaxdir = $(cupsfilterdir)
hpcupsfax_PROGRAMS = hpcupsfax
hpcupsfax_SOURCES = prnt/hpijs/hpcupsfax.cpp prnt/hpijs/hpcupsfax.h common/utils.h common/utils.c
hpcupsfax_LDADD = libhpip.la -lcups -lcupsimage -ldl
endif # FAX_BUILD
endif # HPCUPS ONLY
endif # HPCUPS_INSTALL

# PPDs
ppddir = $(hpppddir)
dist_ppd_DATA = $(foomatic_ps_printers)
if FOOMATIC_PPD_INSTALL
dist_ppd_DATA += $(foomatic_ppd_printers)
endif
if FOOMATIC_DRV_INSTALL
cupsdrvdir = $(drvdir)
cupsdrv_DATA = $(foomatic_drv)
endif
if CUPS_PPD_INSTALL
dist_ppd_DATA += $(cups_ppd_printers)
endif
if CUPS_DRV_INSTALL
cupsdrv2dir = $(drvdir)
cupsdrv2_DATA = $(cups_drv)
endif

install-data-hook:
if HPLIP_BUILD
#	   If scanner build, add hpaio entry to sane dll.conf.
	if [ "$(scan_build)" = "yes" ]; then \
	   $(mkinstalldirs) $(DESTDIR)/etc/sane.d; \
	   if [ ! -f $(DESTDIR)/etc/sane.d/dll.conf ]; then \
		  touch $(DESTDIR)/etc/sane.d/dll.conf; \
	   fi; \
	   if ! grep ^hpaio $(DESTDIR)/etc/sane.d/dll.conf >/dev/null 2>/dev/null ; then \
		  echo "Adding hpaio entry to /etc/sane.d/dll.conf." ; \
		  echo hpaio >>$(DESTDIR)/etc/sane.d/dll.conf ; \
	   fi \
	fi
if FULL_BUILD
#	   Create hp-xxx commands in bindir.
	$(mkinstalldirs) $(DESTDIR)$(bindir)
	for i in $(dist_cmd_SCRIPTS); do \
	   cmd=`basename $$i .py`; \
	   if [ ! \( "$$cmd" = "toolbox" -a "$(gui_build)" = "no" \) ]; then \
		  ln -sf ../share/$(PACKAGE)/$$i $(DESTDIR)$(bindir)/hp-$$cmd; \
	   fi \
	done
#
#	   Install fax support.
	if [ "$(fax_build)" = "yes" ]; then \
	   mv $(DESTDIR)$(hpfaxdir)/hpfax.py $(DESTDIR)$(hpfaxdir)/hpfax; \
	   chmod 700 $(DESTDIR)$(hpfaxdir)/hpfax; \
	fi

	if [ "$(apparmor_build)" = "yes" ];then \
	  chmod 644 $(DESTDIR)$(apparmor_profiledir)/usr.share.hplip;\
	  chmod 644 $(DESTDIR)$(apparmor_profiledir)/abstractions/hplip;\
	fi

#       Make backend run as root.
	chmod 700 $(DESTDIR)$(hpdir)/hp

#AppArmor



endif # FULL_BUILD
endif # HPLIP_BUILD

uninstall-hook:
if HPLIP_BUILD
#	   Remove hp-xxx commands.
	for i in $(dist_cmd_SCRIPTS); do \
		cmd=`basename $$i .py`; \
		rm -f $(DESTDIR)$(bindir)/hp-$$cmd; \
	done
#
#	   Remove fax support.
	rm -f $(DESTDIR)$(hpfaxdir)/hpfax
# 	Remove apparmor profiles
	rm -f $(DESTDIR)$(apparmor_profiledir)/usr.share.hplip
	rm -f $(DESTDIR)$(apparmor_profiledir)/abstractions/hplip
endif

rpm:
if FULL_BUILD
	epm -f rpm hplipfull hplip.list
endif
if LITE_BUILD
	epm -f rpm hpliplite hplip.list
endif
if HPCUPS_ONLY_BUILD
	epm -f rpm hpliphpcupsonly hplip.list
endif
if HPIJS_ONLY_BUILD
	epm -f rpm hpliphpijsonly hplip.list
endif

deb:
if FULL_BUILD
	epm -f deb hplipfull hplip.list
endif
if LITE_BUILD
	epm -f deb hpliplite hplip.list
endif
if HPCUPS_ONLY_BUILD
	epm -f deb hpliphpcupsonly hplip.list
endif
if HPIJS_ONLY_BUILD
	epm -f deb hpliphpijsonly hplip.list
endif
