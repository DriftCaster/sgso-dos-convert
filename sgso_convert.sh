#!/bin/sh
cd "$(dirname "$0")"
python3 -c "import numpy" 2>/dev/null || python3 -m pip install --user numpy
exec python3 sgso_convert.py "$@"
