#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
# (c) Copyright 2008-9 HP Development Company, L.P.
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

__version__ = "3.3"
__title__ = 'DAT to DRV.IN converter. Also creates Foomatic XML files.'
__doc__ = "Create DRV.IN file and Foomatic XML files from MODELS.DAT data. Processes all *.in.template files in prnt/drv directory."

import os
os.putenv("HPLIP_BUILD", "1")

# Std Lib
import os.path
import sys
import getopt
import re
from xml.dom.minidom import Document, parse, parseString
import string

# Local
from base.g import *
from base import utils, tui, models
#from prnt import printable_areas
from base.sixext import text_type
# Globals
errors = 0
count = 0
enc = 'utf-8'

models_dict = {}
norm_models = {} # { 'norm'd model' : ( 'model', type, has_scanner ), ... }
norm_models_keys = {}
model_dat = None
total_models = 0
sorted_category_models = {}
unsupported_models = []

pat_prod_num = re.compile("""(\d+)""", re.I)
pat_template = re.compile("""^(\s*)//\s*<%(\S+)%>""", re.I)
pat_template2 = re.compile("""^\s*<%(\S+)%>""", re.I)


SHORTENING_REPLACEMENTS = {
'color laserjet' : 'CLJ',
'laserjet' : 'LJ',
'photosmart': 'PS',
'deskjet' : 'DJ',
'color inkjet printer' : '',
'officejet' : 'OJ',
'business inkjet' : 'BIJ',
'designjet' : 'DESIGNJ',
'printer scanner copier' : 'PSC',
'color lj' : 'CLJ',
'professional' : 'Pro',
}


USAGE = [(__doc__, "", "name", True),
         ("Usage: dat2drv.py [OPTIONS]", "", "summary", True),
         utils.USAGE_OPTIONS,
         ("Verbose mode:", "-v or --verbose", "option", False),
         ("Quiet mode:", "-q or --quiet", "option", False),
         utils.USAGE_LOGGING1, utils.USAGE_LOGGING2,
         utils.USAGE_HELP,
        ]

def usage(typ='text'):
    if typ == 'text':
        utils.log_title(__title__, __version__)

    utils.format_text(USAGE, typ, __title__, 'drv2xml.py', __version__)
    sys.exit(0)



def _encode(v):
    if isinstance(v, text_type):
        v = v.encode(enc)
    return v



class XMLElement:
    def __init__(self, doc, el):
        self.doc = doc
        self.el = el

    def __getitem__(self, name):
        a = self.el.getAttributeNode(name)
        if a:
            return _encode(a.value)
        return None

    def __setitem__(self, name, value):
        self.el.setAttribute(name, _encode(value))

    def __delitem__(self, name):
        self.el.removeAttribute(name)

    def __str__(self):
        return _encode(self.doc.toxml())

    def toString(self):
        return _encode(self.doc.toxml())

    def _inst(self, el):
        return XMLElement(self.doc, el)

    def get(self, name, default=None):
        a = self.el.getAttributeNode(name)
        if a:
            return _encode(a.value)
        return _encode(default)

    def add(self, tag, **kwargs):
        el = self.doc.createElement(tag)
        for k, v in list(kwargs.items()):
            el.setAttribute(k, _encode(str(v)))
        return self._inst(self.el.appendChild(el))

    def addText(self, data):
        return self._inst(
            self.el.appendChild(
                self.doc.createTextNode(_encode(data))))

    def addComment(self, data):
        return self._inst(
            self.el.appendChild(
                self.doc.createComment(data)))

    def getText(self, sep=" "):
        rc = []
        for node in self.el.childNodes:
            if node.nodeType == node.TEXT_NODE:
                rc.append(node.data)
        return _encode(string.join(rc, sep))

    def getAll(self, tag):
        return list(map(self._inst, self.el.getElementsByTagName(tag)))


class XMLDocument(XMLElement):

    def __init__(self, tag=None, **kwargs):
        self.doc  = Document()
        XMLElement.__init__(self, self.doc, self.doc)
        if tag:
            self.el = self.add(tag, **kwargs).el

    def parse(self, d):
        self.doc = self.el = parse(d)
        return self

    def parseString(self, d):
        self.doc = self.el = parseString(_encode(d))
        return self




def fixFileName(model):
    if model.startswith('hp_'):
        model = model.replace('hp_', 'hp-')

    elif model.startswith('apollo_'):
        model = model.replace('apollo_', 'apollo-')

    elif not model.startswith('hp-'):
        model = 'hp-' + model

    return model.strip('~')


def categorize2(m):
    is_aio = (models_dict[m]['scan-type'] != SCAN_TYPE_NONE or
        models_dict[m]['copy-type'] != COPY_TYPE_NONE or
        models_dict[m]['fax-type'] != FAX_TYPE_NONE)

    if "deskjet" in m or \
        ("color" in m and "inkjet" in m) or \
        m.startswith("dj") or \
        m.startswith("cp"):

        if is_aio:
            i = MODEL_TYPE2_DESKJET_AIO
        else:
            i = MODEL_TYPE2_DESKJET

    elif "photosmart" in m:
        i = MODEL_TYPE2_PHOTOSMART

    elif "officejet" in m:
        i = MODEL_TYPE2_OFFICEJET

    elif "psc" in m or \
        "printer_scanner_copier" in m:

        i = MODEL_TYPE2_PSC

    elif "laserjet" in m:
        if "color" in m:
            i = MODEL_TYPE2_COLOR_LASERJET
        else:
            i = MODEL_TYPE2_LASERJET

    elif "mopier" in m:
        i = MODEL_TYPE2_LASERJET

    elif "business" in m and \
        "inkjet" in m:

        i = MODEL_TYPE2_BIJ

    elif "edgeline" in m:
        i = MODEL_TYPE2_EDGELINE

    elif "apollo" in m:
        i = MODEL_TYPE2_APOLLO

    elif "designjet" in m or \
         "plotter" in m or \
         "draft" in m or \
         "eagle" in m or \
         "electrostatic" in m or \
         m.startswith('hp_2') or \
         m.startswith('hp_7') or \
         m.startswith('hp_9'):

        i = MODEL_TYPE2_DESIGNJET

    elif "scanjet" in m:
        i = MODEL_TYPE2_SCANJET

    else: # Other
        i = MODEL_TYPE2_OTHER

    return (m, i, models_dict[m])


def sort_product(x, y):
    try:
        _x = int(pat_prod_num.search(x).group(1))
    except (TypeError, AttributeError):
        _x = 0

    try:
        _y = int(pat_prod_num.search(y).group(1))
    except (TypeError, AttributeError):
        _y = 0

    if not _x and not _y:
        return cmp(x, y)

    return cmp(_x, _y)


def sort_product2(x, y): # sort key is first element of tuple
    return sort_product(x[0], y[0])


def load_models(unreleased=True):
    global models_dict
    global norm_models
    global norm_models_keys
    global model_dat
    global total_models
    global sorted_category_models
    global unsupported_models

    models_dict = model_dat.read_all_files(unreleased)

    log.debug("Raw models:")

    for m in models_dict:
        nm = models.normalizeModelUIName(m)
        models_dict[m]['norm_model'] = nm.strip('~')
        models_dict[m]['case_models'] = []

        i, case_models = 1, []
        while True:
            try:
                cm = models.normalizeModelUIName(models_dict[m]['model%d' % i])
            except KeyError:
                break

            case_models.append(cm)
            i+= 1

        if not case_models:
            case_models = [nm]

        models_dict[m]['case_models'] = case_models[:]
        cat = categorize2(m)
        models_dict[m]['category'] = cat

        for c in case_models:
            norm_models[c] =  cat

            if models_dict[m]['support-type'] == SUPPORT_TYPE_NONE:
                unsupported_models.append((c, m))

    norm_models_keys = list(norm_models.keys())
    try:
        norm_models_keys.sort(key=lambda y: pat_prod_num.search(y).group(1))
    except:
        norm_models_keys.sort(key=str.lower)

    unsupported_models.sort(key= lambda x: x[0])
 

    total_models = len(norm_models)

    #log.info("Loaded %d models." % total_models)




def main(args):
    global errors
    global model_dat
    line_num = 0
    log.set_module("dat2drv.py")
    cur_path = os.path.realpath(os.path.normpath(os.getcwd()))
    dat_path = os.path.join(cur_path, 'data', 'models')
    model_dat = models.ModelData(dat_path)
    load_models()



    verbose = False
    quiet = False

    try:
        opts, args = getopt.getopt(args, 'd:l:ho:vq',
                                   ['logging=', 'help',
                                    'help-rest', 'help-man',
                                    'drv=', 'output=',
                                    'verbose', 'quiet'])
    except getopt.GetoptError as e:
        log.error(e.msg)
        usage()
        sys.exit(0)

    log_level = 'info'
    if os.getenv("HPLIP_DEBUG"):
        log.set_level('debug')

    for o, a in opts:
        if o in ('-h', '--help'):
            usage()

        elif o == '--help-rest':
            usage('rest')

        elif o == '--help-man':
            usage('man')

        elif o in ('-v', '--verbose'):
            verbose = True

        elif o in ('-q', '--quiet'):
            quiet = True

        elif o in ('-l', '--logging'):
            log.set_level(a.lower().strip())


    if not quiet:
        utils.log_title(__title__, __version__)

    drv_dir = os.path.join(cur_path, 'prnt', 'drv')

    errors = []
    warns = []
    notes = []

    for template_file in utils.walkFiles(drv_dir, recurse=False, abs_paths=True,
        return_folders=False, pattern='*.in.template'):

        basename = os.path.basename(template_file).split('.')[0]

        # Output
        drv_in_file = os.path.join(cur_path, 'prnt', 'drv', '%s.drv.in' % basename)

        # XML output (per model)
        output_path = os.path.join(cur_path, 'prnt', 'drv', 'foomatic_xml', basename)

        # XML Output (master driver list)
        driver_path = os.path.join(cur_path, 'prnt', 'drv', 'foomatic_xml', basename, '%s.xml' % basename)

        log.info("Working on %s file..." % basename)
        log.info("Input file: %s" % template_file)
        log.info("Output file: %s" % drv_in_file)
        log.info("Output XML directory: %s" % output_path)
        log.info("Output driver XML file: %s" % driver_path)



        # CREATE DRV.IN FILE

        log.info("Processing %s.drv.in.template..." % basename)
        tui.update_spinner()

        template_classes = []

        template_file_f = open(template_file, 'r')
        drv_in_file_f = open(drv_in_file, 'w')

        models_placement = {}
        for m in models_dict:
            models_placement[m] = 0

        line = 0

        for x in template_file_f:
            if verbose:
                log.info(x.strip())

            line += 1
            tui.update_spinner()
            drv_in_file_f.write(x)
            match = pat_template.match(x)
            if match is not None:
                matches = []
                indent = match.group(1)
                indent2 = ' '*(len(indent)+2)

                classes = match.group(2).split(':')
                tech_class = classes[0]

                if tech_class not in models.TECH_CLASSES:
                    errors.append("(%s:line %d) Invalid tech-class (%s): %s" % (basename, line, tech_class, x.strip()))
                    continue

                template_classes.append(tech_class)

                tech_subclass = classes[1:]

                ok = True
                for sc in tech_subclass:
                    if sc not in models.TECH_SUBCLASSES:
                        errors.append("(%s:line %d) Invalid tech-subclass (%s): %s" % (basename, line, sc, x.strip()))
                        ok = False

                if not ok:
                    continue

                for m in models_dict:
                    include = False

                    if tech_class in models_dict[m]['tech-class'] and \
                        len(models_dict[m]['tech-subclass']) == len(tech_subclass):

                        for msc in models_dict[m]['tech-subclass']:
                            if msc not in tech_subclass:
                               break
                        else:
                            include = True

                    if include:
                        models_placement[m] += 1
                        matches.append(m)

                if matches:
                    try:
                        matches.sort(key=lambda y: pat_prod_num.search(y).group(1))
                    except:
                        matches.sort(key=str.lower)
                    for p in matches:

                        if verbose:
                            log.info("(%s) Adding section for model: %s" % (basename, p))

                        drv_in_file_f.write("%s{\n" % indent)

                        if basename == 'hpcups':
                            model_name = models_dict[p]['norm_model']
                        else:
                            model_name = models_dict[p]['norm_model'] + " %s" % basename

                        orig_model_name = model_name

                        while True:
                            if len(model_name) > 31:
                                for k in SHORTENING_REPLACEMENTS:
                                    if k in model_name.lower():
                                        model_name = utils.ireplace(model_name, k, SHORTENING_REPLACEMENTS[k])
                                        model_name = model_name.replace('  ', ' ')

                                        if len(model_name) < 32:
                                            warns.append('len("%s")>31, shortened to len("%s")=%d using sub-brand shortening replacements.' % (orig_model_name, model_name, len(model_name)))
                                            break

                                if len(model_name) < 32:
                                    break

                                if "series" in model_name.lower():
                                    model_name = utils.ireplace(model_name, "series", "Ser.")
                                    model_name = model_name.replace('  ', ' ')

                                    if len(model_name) < 32:
                                        warns.append('len("%s")>31, shortened to len("%s")=%d using "series" to "ser." replacement.' % (orig_model_name, model_name, len(model_name)))
                                        break

                                if "ser." in model_name.lower():
                                    model_name = utils.ireplace(model_name, "ser.", "")
                                    model_name = model_name.replace('  ', ' ')

                                    if len(model_name) < 32:
                                        warns.append('len("%s")>31, shortened to len("%s")=%d using "ser." removal.' % (orig_model_name, model_name, len(model_name)))
                                        break

                                if len(model_name) > 31:
                                    model_name = model_name[:31]
                                    errors.append('len("%s")>31 chars, could not shorten to <32. Truncating to 31 chars (%s).' % (orig_model_name, model_name))

                            break

                        drv_in_file_f.write('%sModelName "%s"\n' % (indent2, orig_model_name))

                        if len(models_dict[p]['tech-class']) > 1:
                            if basename == "hpcups":
                                drv_in_file_f.write('%sAttribute "NickName" "" "%s %s, %s $Version' %
                                    (indent2, orig_model_name, models.TECH_CLASS_PDLS[tech_class],basename))
                            else:
                                drv_in_file_f.write('%sAttribute "NickName" "" "%s %s, $Version' %
                                    (indent2, orig_model_name, models.TECH_CLASS_PDLS[tech_class]))
                        else:
                            if basename == "hpcups":
                                drv_in_file_f.write('%sAttribute "NickName" "" "%s, %s $Version' %
                                    (indent2, orig_model_name, basename))
                            else:
                                drv_in_file_f.write('%sAttribute "NickName" "" "%s, $Version' %
                                    (indent2, orig_model_name))
                        if models_dict[p]['plugin'] in (1, 2):
                            if (models_dict[p]['plugin-reason'] & 15 ) in (1, 2, 3, 4, 5, 6, 8, 9, 10, 12):
                                drv_in_file_f.write(', requires proprietary plugin')

                        drv_in_file_f.write('"\n')

                        drv_in_file_f.write('%sAttribute "ShortNickName" "" "%s"\n' % (indent2, model_name))

                        pp = p.replace('_', ' ')
                        if 'apollo' in p.lower():
                            devid = "MFG:Apollo;MDL:%s;DES:%s;" % (pp, pp)
                        elif 'laserjet' in p.lower() or 'designjet' in p.lower():
                            devid = "MFG:Hewlett-Packard;MDL:%s;DES:%s;" % (pp, pp)
                        else:
                            devid = "MFG:HP;MDL:%s;DES:%s;" % (pp, pp)

                        drv_in_file_f.write('%sAttribute "1284DeviceID" "" "%s"\n' % (indent2, devid))

                        if len(models_dict[p]['tech-class']) > 1:
                            if basename == 'hpcups':
                                drv_in_file_f.write('%sPCFileName "%s-%s.ppd"\n' %
                                    (indent2, fixFileName(p), models.TECH_CLASS_PDLS[tech_class]))
                            else:
                                drv_in_file_f.write('%sPCFileName "%s-%s-%s.ppd"\n' %
                                    (indent2, fixFileName(p), basename, models.TECH_CLASS_PDLS[tech_class]))

                        elif tech_class != 'Postscript':
                            if basename == 'hpcups':
                                drv_in_file_f.write('%sPCFileName "%s.ppd"\n' % (indent2, fixFileName(p)))
                            else:
                                drv_in_file_f.write('%sPCFileName "%s-%s.ppd"\n' % (indent2, fixFileName(p), basename))

                        else:
                            drv_in_file_f.write('%sPCFileName "%s-ps.ppd"\n' % (indent2, fixFileName(p)))

                        for c in models_dict[p]['case_models']:
                            drv_in_file_f.write('%sAttribute "Product" "" "(%s)"\n' % (indent2, c))

                        drv_in_file_f.write("%s}\n" % indent)

                else:
                    errors.append("(%s:line %d) No models matched the specified classes on line: %s" % (basename, line, x.strip()))

            else:
                match = pat_template2.match(x)
                if match is not None:
                    errors.append("(%s:line %d) Malformed line: %s (missing initial //)" % (basename, line, x.strip()))


        template_file_f.close()
        drv_in_file_f.close()
        tui.cleanup_spinner()

        for tc in models.TECH_CLASSES:
            if tc.lower() in ('undefined', 'postscript', 'unsupported'):
                continue

            if tc not in template_classes:
                warns.append("(%s) Section <%%%s:...%%> not found." % (basename, tc))


        # OUTPUT XML FILES

        if not os.path.exists(output_path):
            os.makedirs(output_path)

        if os.path.exists(driver_path):
            os.remove(driver_path)

        files_to_delete = []
        for f in utils.walkFiles(output_path, recurse=True, abs_paths=True, return_folders=False, pattern='*'):
            files_to_delete.append(f)

        for f in files_to_delete:
            os.remove(f)

        driver_f = open(driver_path, 'w')


        driver_doc = XMLDocument("driver", id="driver/hplip")
        name_node = driver_doc.add("name")
        name_node.addText("hplip")
        url_node = driver_doc.add("url")
        url_node.addText("http://hplipopensource.com")
        supplier_node = driver_doc.add("supplier")
        supplier_node.addText("Hewlett-Packard")
        mfg_node = driver_doc.add("manufacturersupplied")
        mfg_node.addText("HP|Apollo")
        lic_node = driver_doc.add("license")
        lic_node.addText("BSD/GPL/MIT")
        driver_doc.add("freesoftware")
        support_node = driver_doc.add("supportcontact", level="voluntary", url="https://launchpad.net/hplip")
        support_node.addText("HPLIP Support at Launchpad.net")
        shortdesc_node = driver_doc.add("shortdescription")
        shortdesc_en_node = shortdesc_node.add("en")
        shortdesc_en_node.addText("HP's driver suite for printers and multi-function devices")
        func_node = driver_doc.add("functionality")
        maxresx_node = func_node.add("maxresx")
        maxresx_node.addText("1200")
        maxresy_node = func_node.add("maxresy")
        maxresy_node.addText("1200")
        func_node.add("color")
        exec_node = driver_doc.add("execution")
        exec_node.add("nopjl")
        exec_node.add("ijs")
        proto_node = exec_node.add("prototype")
        #proto_node.addText("gs -q -dBATCH -dPARANOIDSAFER -dQUIET -dNOPAUSE -sDEVICE=ijs -sIjsServer=hpijs%A%B%C -dIjsUseOutputFD%Z -sOutputFile=- -")
        comments_node = driver_doc.add("comments")
        comments_en_node = comments_node.add("en")
        comments_en_node.addText("")

        printers_node = driver_doc.add("printers")

        for m in models_dict:

            if models_dict[m]['support-type'] == SUPPORT_TYPE_NONE:
                continue

            if 'apollo' in m.lower():
                make = 'Apollo'
            else:
                make = 'HP'

            if 'apollo' in m.lower():
                ieee1284 = "MFG:Apollo;MDL:%s;DES:%s;" % (m, m)

            else:
                ieee1284 = "MFG:HP;MDL:%s;DES:%s;" % (m, m)

            postscriptppd = ''
            if 'Postscript' in models_dict[m]['tech-class']:
                postscriptppd = "%s-ps.ppd" % fixFileName(m)

            stripped_model = m

            if stripped_model.startswith('hp_'):
                stripped_model = stripped_model.replace('hp_', '').capitalize()

            elif stripped_model.startswith('apollo_'):
                stripped_model = stripped_model.replace('apollo_', '').capitalize()

            fixed_model = stripped_model.replace('_', ' ').capitalize()

            # Output to the per-model XML file
            outputModel(m, fixed_model, stripped_model, make, postscriptppd, ieee1284, output_path, verbose)

            # Output to driver master XML file
            outputDriver(m, fixed_model, stripped_model, make, printers_node, verbose)

        driver_f.write(str(driver_doc))
        driver_f.close()

        # Make sure all models ended up in drv.in file
        log.info("Checking for errors...")
        tui.update_spinner()

        for m in models_dict:
            tui.update_spinner()
            tc = models_dict[m]['tech-class']
            st = models_dict[m]['support-type']

            if not tc or 'Undefined' in tc:
                if st:
                    errors.append('(%s) Invalid tech-class for model %s ("Undefined" or missing)' % (basename, m))
                #else:
                #    warns.append('(%s) Invalid tech-class for unsupported model %s ("Undefined" or missing)' % (basename, m))

            else:
                if not models_placement[m] and st and \
                    len(tc) == 1 and 'Postscript' not in tc:

                    sects = []
                    for tc in models_dict[m]['tech-class']:
                        for sc in models_dict[m]['tech-subclass']:
                            sects.append(sc)

                    errors.append("(%s) Model '%s' did not have a matching section. Needed section: <%%%s:%s%%>" %
                        (basename, m, tc, ':'.join(sects)))

                if len(tc) == 1 and 'Postscript' in tc:
                    notes.append("(%s) Postscript-only model %s was not included in DRV file." % (basename, m))

        tui.cleanup_spinner()

        # end for

    if not quiet or verbose:
        if notes:
            tui.header("NOTES")
            for n in notes:
                log.note(n)

        if warns:
            tui.header("WARNINGS")
            for w in warns:
                log.warn(w)

        if errors:
            tui.header("ERRORS")
            for e in errors:
                log.error(e)

    else:
        if warns:
            log.warn("%d warnings" % len(warns))

        if errors:
            log.error("%d errors" % len(errors))


def parseDeviceID(device_id):
    d= {}
    x = [y.strip() for y in device_id.strip().split(';') if y]

    for z in x:
        y = z.split(':')
        try:
            d.setdefault(y[0].strip(), y[1])
        except IndexError:
            d.setdefault(y[0].strip(), None)

    d.setdefault('MDL', '')
    d.setdefault('SN',  '')

    if 'MODEL' in d:
        d['MDL'] = d['MODEL']
        del d['MODEL']

    if 'SERIAL' in d:
        d['SN'] = d['SERIAL']
        del d['SERIAL']

    elif 'SERN' in d:
        d['SN'] = d['SERN']
        del d['SERN']

    if d['SN'].startswith('X'):
        d['SN'] = ''

    return d


def outputModel(model, fixed_model, stripped_model, make, postscriptppd, ieee1284, output_path, verbose=False):
    global errors
    global count

    count += 1

##    fixed_model = model.replace(' ', '_')
##
##    if fixed_model.startswith('hp_'):
##        fixed_model = fixed_model.replace('hp_', 'hp-')
##
##    elif fixed_model.startswith('apollo_'):
##        fixed_model = fixed_model.replace('apollo_', 'apollo-')
##
##    else:
##        fixed_model = 'hp-' + fixed_model
##
##    stripped_model = model
##    if stripped_model.startswith('hp '):
##        stripped_model = stripped_model.replace('hp ', '')


    printerID = make + '-' + stripped_model

    output_filename = os.path.join(output_path, printerID+".xml")

    if verbose:
        log.info("\n\n%s:" % output_filename)

    output_f = open(output_filename, 'w')

    doc = XMLDocument("printer", id="printer/%s" % printerID)
    make_node = doc.add("make")
    make_node.addText(make)
    model_node = doc.add("model")
    model_node.addText(fixed_model)
    url_node = doc.add("url")
    url_node.addText("http://www.hp.com")

    lang_node = doc.add("lang")
    lang_node.add("pcl", level="3")

    autodetect_node = doc.add("autodetect")
    usb_node = autodetect_node.add("usb")

    driver_node = doc.add("driver")
    driver_node.addText('hplip')

    drivers_node = doc.add("drivers")
    driver_node = drivers_node.add("driver")
    id_node = driver_node.add("id")
    id_node.addText("hplip")

    if postscriptppd:
        # Postscript
        lang_node.add("postscript", level="2")
        lang_node.add("pjl")
        text_node = lang_node.add("text")
        charset_node = text_node.add("charset")
        charset_node.addText("us-ascii")
        #ppd_node = driver_node.add("ppd")
        #ppd_node.addText(postscriptppd)
    #else:
    #    id_node.addText("hpijs")

    if 1:
        #ieee1284_node = usb_node.add("ieee1284")
        #ieee1284_node.addText(ieee1284)

        device_id = parseDeviceID(ieee1284)

        desc_node = usb_node.add("description")
        #desc_node.addText(device_id['DES'])
        desc_node.addText(make + ' ' + fixed_model)

        mfg_node = usb_node.add("manufacturer")
        #mfg_node.addText(device_id['MFG'])
        mfg_node.addText("Hewlett-Packard")

        model_node = usb_node.add("model")
        #model_node.addText(device_id['MDL'])
        model_node.addText(make + ' ' + fixed_model)

        #cmdset_node = usb_node.add("commandset")
        #cmdset_node.addText("???")

    if verbose:
        log.info(str(doc))

    output_f.write(str(doc))

    output_f.close()


def outputDriver(m, fixed_model, stripped_model, make, printers_node, verbose):

    printerID = make + '-' + stripped_model

    tech_classes = models_dict[m]['tech-class']
    #print tech_classes
    printer_node = printers_node.add("printer")
    id_node = printer_node.add("id")
    id_node.addText("printer/%s" % printerID)

##    margins_node = printer_node.add("margins")
##    general_margins_node = margins_node.add("general")

##    unit_node = general_margins_node.add("unit")
##    unit_node.addText("in")
##
##    for tc in tech_classes:
##        if tc not in ('Undefined', 'Unsupported', 'PostScript'):
##            try:
##                margins_data = printable_areas.data[tc]
##            except KeyError:
##                continue
##            else:
##                print margins_data
##                break

##<printer>
##   <id>printer/HP-DeskJet_350C</id><!-- HP DeskJet 350C -->
##   <functionality>
##    <maxresx>600</maxresx>
##    <maxresy>300</maxresy>
##   </functionality>
##   <ppdentry>
##     *DefaultResolution: 600dpi
##    </ppdentry>
##   <margins>
##    <general>
##     <unit>in</unit>
##     <relative />
##     <left>0.25</left>
##     <right>0.25</right>
##     <top>0.125</top>
##     <bottom>0.67</bottom>
##    </general>
##    <exception PageSize="A4">
##     <left>0.135</left>
##     <right>0.135</right>
##    </exception>
##   </margins>
##  </printer>



if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))

