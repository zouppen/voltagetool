Data dumper for DCDC USB power supply unit
==========================================

Device description: 

http://www.mini-box.com/DCDC-USB?sc=8&category=981

Author
------

Joel Lehtonen, joel.lehtonen ät iki.fi. Feel free to contact!

License
-------

GNU GPL version 2 or (at your option) later. COPYING missing at the moment. :-)

Requirements:
-------------

- Linux 2.6
- gcc (build time)
- scons (build time)

Usage
-----

To compile, type 'scons'. If command is not found, please install
package 'scons' with apt-get or similar package tool first.

To get some information:

$ sudo ./debugtool

To set output voltage (VOUT):

$ sudo ./debugtool 13.5

To monitor voltage, measurement every 10.5 seconds:

$ sudo ./voltagemonitor 10.5

TODO
----

A polished and elegant tool. This is a piece of crappiness.

Thanks
------

Idea and some code I got from:

http://www.ros.org/wiki/minibox_dcdc

Thank you ROS. I'd like to see you to do Linux kernel driver for
it. But I may do it some day if I've got enough time.

Thanks to Minibox for the original Windows sources, too. It would have
been very tricky to do voltage conversions without the original
implementation
