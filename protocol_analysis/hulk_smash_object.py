#!/usr/bin/python3 -B
# -*- coding: utf-8 -*-

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP

for i in range(0,65536):
    #          read operation                     object number [0-65535]            property
    message = (1).to_bytes(2, byteorder='big') + (i).to_bytes(2, byteorder='big') + (0).to_bytes(2, byteorder='big')
    sock.sendto(message, ("0.0.0.0", 4000))

