env = Environment()

# add support for GTK
env.ParseConfig('pkg-config --cflags --libs libusb')

env.Object('usbtools.c')
env.Program(['debugtool.c', 'usbtools'])
