import sys
import charts_handler
from traceback import print_exc
from shlex import split
import os
import freeOrionAIInterface as fo

from freeorion_debug.option_tools import get_option_dict, HANDLERS

handlers = split(get_option_dict()[HANDLERS])

for handler in handlers:
    module = os.path.basename(handler)[:-3]
    if os.path.exists(handler):
        module_path = os.path.dirname(handler)
    else:
        module_path = os.path.join(fo.getAIDir(), os.path.dirname(handler))

    sys.path.insert(0, module_path)
    try:
        __import__(module)
    except Exception as e:
        for p in sys.path:
            print p
        print >> sys.stderr, "Fail to import handler %s with error %s" % (handler, e)
        print_exc()
        exit(1)
