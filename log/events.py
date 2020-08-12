#coding=utf-8
#!/usr/bin/env python

import string
import sys

from ESL import *

con = ESLconnection("127.0.0.1","8021","ClueCon")
#are we connected?

if con.connected:

  con.events("plain", "CUSTOM CUSTOM::nway nway::info nway::queue");

  while 1:
  #my $e = $con->recvEventTimed(100);
    e = con.recvEvent()

    if e:
      print e.serialize()