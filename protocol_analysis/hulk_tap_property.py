#!/usr/bin/python3 -B
# -*- coding: utf-8 -*-
 
import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP

#          read operation                     object number                      property
message = (1).to_bytes(2, byteorder='big') + (1).to_bytes(2, byteorder='big') + (255).to_bytes(2, byteorder='big')
sock.sendto(message, ("0.0.0.0", 4000))

