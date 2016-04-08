#!/bin/bash

PYTHON="python"

if [ ! -f /usr/bin/python -a -f /usr/bin/python3 ]; then
	PYTHON="python3"
	PAT1="#!\/usr\/bin\/(env )?python$"
	PAT2="#!\/usr\/bin\/env python3"
	#replace shebang lines to python3 interpreter
	for pyfile in `find . -maxdepth 1 -name "*.py"`; do
		sed -i -r "0,/$PAT1/s//$PAT2/" $pyfile
	done
	for pyfile in `find . -type f \( -iname hpps -o -iname hpfax.py -o -iname pstotiff \)`; do
		sed -i -r "0,/$PAT1/s//$PAT2/" $pyfile
	done
fi

MAJOR=`$PYTHON -c 'import sys; print(sys.version_info[0])'`
MINOR=`$PYTHON -c 'import sys; print(sys.version_info[1])'`
if [ "$MAJOR" -le 2 ] && [ "$MINOR" -lt 6 ];then
	echo -e "\e[1;31mInstalled python version is ${MAJOR}.${MINOR}\e[0m"
	echo -e "\e[1;31mThis installer cannot be run on python version < 2.6\e[0m"
	echo -e "\e[1;31mPlease download the appropriate hplip version form www.hplipopensource.com\e[0m"
	exit 0
else
	./install.py -i $*
fi
