#!/usr/bin/env python

import sys
import time
from socket import *
from os import environ
import cgi
import cgitb

cgitb.enable()

# print("Entered Script!")

time.sleep(20)

print('woke up from sleeping!')

try:
    sys.stdout.close()
except:
    pass
try:
    sys.stderr.close()
except:
    pass
