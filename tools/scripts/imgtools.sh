#!/bin/bash
#echo $@
export PATH="$(cd "$(dirname "${BASH_SOURC[0]}")" && pwd)/bin":$PATH
python3 mk_image.py $@
