#!/usr/bin/env python

import sys
from socket import *
from os import environ

serverHost = gethostbyname("http://artii.herokuapp.com")
serverPort = 80

s = socket(AF_INET, SOCK_STREAM)
s.connect((serverHost, serverPort))

for key in environ:
    value = environ[key]
    print "'%s' : '%s'" % (key, value)

print "my stdin: '%s'" % (sys.stdin.read())

uri = "/make?" + environ['QUERY_STRING']

host = "Host: " + environ['HTTP_HOST']

connection = "Connection: " + environ['HTTP_CONNECTION']

request = "GET " + uri + " HTTP/1.1\r\n" + host + "\r\n" + connection + "\r\n\r\n"

print(request)
s.send(request)

data = s.recv(8192)

print(data)
