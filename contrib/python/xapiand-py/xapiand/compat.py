import sys

PY2 = sys.version_info[0] == 2

if PY2:
    text_type = unicode
    binary_type = str
    string_types = basestring,
    from urllib import quote_plus, urlencode, unquote
    from urlparse import  urlparse
    from itertools import imap as map
    from Queue import Queue
else:
    text_type = str
    binary_type = bytes
    string_types = str, bytes
    from urllib.parse import quote_plus, urlencode, urlparse, unquote
    map = map
    from queue import Queue
