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
string = 'POST /malloc.txt HTTP/1.1\r\nContent-Length: 100\r\n\r\n\
This is what I want to send!\n'
tls.sendall(string)
print tls.recv(4096)
tls.close()



# try another connection!
sock = socket.create_connection(('localhost', 8081))
tls = ssl.wrap_socket(sock, cert_reqs=ssl.CERT_REQUIRED,
                            ca_certs='../certs/signer.crt',
                            ssl_version=ssl.PROTOCOL_TLSv1)


tls.sendall('GET /malloc.txt HTTP/1.1\r\nUser-Agent: 441UserAgent/1.0.0\r\n\r\n')
print tls.recv(4096)
tls.close()

exit(0)
