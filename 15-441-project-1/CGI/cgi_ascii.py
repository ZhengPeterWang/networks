#!/usr/bin/env python

import sys
from socket import *
from os import environ
import cgi
import cgitb

cgitb.enable()

# print("Entered Script!")

serverHost = gethostbyname("artii.herokuapp.com")
serverPort = 80

s = socket(AF_INET, SOCK_STREAM)
s.connect((serverHost, serverPort))

# for key in environ:
#     value = environ[key]
#     print "'%s' : '%s'" % (key, value)

# print "my stdin: '%s'" % (sys.stdin.read())

uri = "/make?" + environ['QUERY_STRING']

host = "Host: artii.herokuapp.com"

connection = "Connection: " + environ['HTTP_CONNECTION']

request = "GET " + uri + " HTTP/1.1\r\n" + host + "\r\n" + connection + "\r\n\r\n"

sys.stderr.write(request)

s.send(request)

data = s.recv(8192)

print(data)

try:
    sys.stdout.close()
except:
    pass
try:
    sys.stderr.close()
except:
    pass
