#!/usr/bin/env python
#
# This script serves as a simple TLSv1 client for 15-441
#
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,
#          Charles Rang <rang972@gmail.com>,
#          Wolfgang Richter <wolf@cs.cmu.edu>

import pprint
import socket
import ssl

# try a connection
sock = socket.create_connection(('localhost', 8081))
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='../certs/signer.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)

# what cert did he present?
pprint.pprint(tls.getpeercert())
string = 'POST /malloc.txt HTTP/1.1\r\nContent-Length: 9800\r\n\r\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n\
This is what I want to send mod 1!\n\
This is what I want to send mod 2!\n\
This is what I want to send mod 3!\n\
This is what I want to send mod 4!\n\
This is what I want to send mod 5!\n\
This is what I want to send mod 6!\n\
This is what I want to send mod 7!\n\
This is what I want to send mod 8!\n\
This is what I want to send mod 9!\n\
This is what I want to send mod 0!\n'
text_file = open("test.txt", "w")
text_file.write(string)
text_file.close()
tls.sendall(string)
print tls.recv(4096)
tls.close()



# try another connection!
sock = socket.create_connection(('localhost', 8081))
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='../certs/signer.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)


tls.sendall('GET /malloc.txt HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n')
print tls.recv(10006)
tls.close()

exit(0)
