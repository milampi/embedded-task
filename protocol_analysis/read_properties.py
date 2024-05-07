#!/usr/bin/python3 -B
# -*- coding: utf-8 -*-

import socket

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # UDP

# 1.14:  enabled=1
# 1.170: amplitude=5000
# 1.255: frequency=500
# 1.300: glitch_chance=60
# 2.14:  enabled=1
# 2.170: amplitude=5000
# 2.255: frequency=250
# 2.300: glitch_chance=5
# 3.14:  enabled=1
# 3.42:  min_duration=1000
# 3.43:  max_duration=5000

for i in range(1,4):
    message = (1).to_bytes(2, byteorder='big') + (i).to_bytes(2, byteorder='big') + (14).to_bytes(2, byteorder='big')
    sock.sendto(message, ("0.0.0.0", 4000))
    message = (1).to_bytes(2, byteorder='big') + (i).to_bytes(2, byteorder='big') + (170).to_bytes(2, byteorder='big')
    sock.sendto(message, ("0.0.0.0", 4000))
    message = (1).to_bytes(2, byteorder='big') + (i).to_bytes(2, byteorder='big') + (255).to_bytes(2, byteorder='big')
    sock.sendto(message, ("0.0.0.0", 4000))
    message = (1).to_bytes(2, byteorder='big') + (i).to_bytes(2, byteorder='big') + (300).to_bytes(2, byteorder='big')
    sock.sendto(message, ("0.0.0.0", 4000))

