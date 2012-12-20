# UDP server example
import socket
import os
import struct

server_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_socket.bind(("", 4223))

print"UDPServer Waiting for client on port 4223"

while 1:
  data, address = server_socket.recvfrom(256)
  can_dez_id, can_dez_dlc, can_dez_data = (struct.unpack('>IBQ',data))

  if can_dez_dlc > 8:
    can_dez_dlc = 8;
    print "warning: can_dez_dlc > 8"

  if can_dez_id & 0x80000000:
    can_id = "%08X" % (can_dez_id & 0x1FFFFFFF)
  else:
    can_id = "%03X" % (can_dez_id & 0x7FF)
  can_data = ("%016X" % can_dez_data)[:can_dez_dlc*2]

  print "./cansend vcan0 " + can_id + "#" + can_data
  os.system("./cansend vcan0 " + can_id + "#" + can_data)
  print "prio: %i vlan: %i src: %i dst: %i prot: %i" % (can_dez_id>>26&0x7, can_dez_id>>22&0xF, can_dez_id>>14&0xFF, can_dez_id>>06&0xFF, can_dez_id>>0&0x3F)
  