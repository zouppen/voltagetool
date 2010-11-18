#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usb.h>

#include "usbtools.h"

int dcdc_init(struct dcdc_cfg *cfg, int debug)
{
	cfg->debug = debug;
	cfg->stop_request = 0;
	cfg->dev = NULL;

	usb_init ();
	usb_set_debug(0);
	usb_find_busses();
	usb_find_devices();

	if (cfg->debug) warnx("Looking for device DCDC-USB");
	
	struct usb_bus *bus;
	
	for (bus = usb_get_busses (); bus != NULL; bus = bus->next) {
		struct usb_device *tmp_dev;

		for (tmp_dev = bus->devices; tmp_dev != NULL;
		     tmp_dev = tmp_dev->next) {
			
			if ((tmp_dev->descriptor.idVendor  == 0x04d8) &&
			    (tmp_dev->descriptor.idProduct == 0xd003)) {

				if (cfg->debug) warnx("Found DCDC-USB device");
				
				cfg->dev = usb_open(tmp_dev);
				break;
			}
		}
	}

	if (cfg->dev == NULL) {
		if (cfg->debug) warnx("no device found");
		return DCDC_NO_DEVICE;
	}

	if (cfg->debug) warnx("opening device succeeded");
	
        char driver[1024];
        if (usb_get_driver_np (cfg->dev, 0, driver, sizeof (driver)) == 0)
        {
		if (cfg->debug)
			warnx("Interface already claimed by '%s' detaching.",
			      driver);
		
		if ((usb_detach_kernel_driver_np (cfg->dev, 0) < 0)) {
			if (cfg->debug) warnx("unable to detach %s driver",
					      driver);
			return DCDC_DETACH_ERROR;
		}
		
		usb_reset(cfg->dev);
		
		int error = 0;
		error = usb_set_configuration (cfg->dev, 1);
		if(error != 0 && cfg->debug)
			warnx("unable to set configuration: %d", error);
		
		if ((error = usb_claim_interface (cfg->dev, 0)) < 0)
		{
			if (cfg->debug)
				warnx("Cannot claim interface! error=%d",error);
			
			usb_close (cfg->dev);
		}
		usleep (100);
		if ((error = usb_set_altinterface (cfg->dev, 0)) < 0 &&
		    cfg->debug)
			warnx("unable to set alternate configuration: %d",
			      error);
		
		char buf[65535];
		error = usb_control_msg(cfg->dev,
					USB_TYPE_CLASS + USB_RECIP_INTERFACE,
					0x000000a, 0x0000000, 0x0000000,
					buf, 0x0000000, 1000);
		if(error != 0 && cfg->debug)
			warnx("unable to send control message: %d", error );
		
	}
	
	if (cfg->debug) warnx("init complete");

	return DCDC_SUCCESS;
}

void dcdc_stop(struct dcdc_cfg *cfg)
{
      usleep(1e6);

      usb_release_interface (cfg->dev, 0);
      usb_close(cfg->dev);

      if (cfg->debug) warnx("close complete");
}

int send(struct dcdc_cfg *cfg, unsigned char *data, int size)
{
	if (data == NULL)
		return -1;

	int error = usb_interrupt_write (cfg->dev, USB_ENDPOINT_OUT + 1,
					 (char *) data, size, 1000);
	if( error != size && cfg->debug )
		warnx("error on usb_interrupt_write, returned code %d", error);

	return error;
}

int receive(struct dcdc_cfg *cfg, unsigned char *data, int length, int timeout)
{
	if (data == NULL)
		return -1;

	int error = usb_interrupt_read (cfg->dev, USB_ENDPOINT_IN + 1,
					(char *)data, length, timeout);
	
	if ( error != length && cfg->debug )
		warnx("error on usb_interrupt_read, returned code %d", error);
	
	return error;
}

int get_all_values(struct dcdc_cfg *cfg)
{
      unsigned char packet[3];

      packet[0] = DCDCUSB_GET_ALL_VALUES;
      packet[1] = 0;

      return send(cfg, packet, 2);
}

int send_command(struct dcdc_cfg *cfg, uint8_t command, uint8_t value)
{
      unsigned char packet[5];

      packet[0] = DCDCUSB_CMD_OUT;
      packet[1] = command;
      packet[2] = value;
      packet[3] = 0;
      packet[4] = 0;

      return send(cfg, packet, 5);
}

int read_status(struct dcdc_cfg *cfg, uint8_t * data)
{
      unsigned char packet[24];
      int len, i;
      int error;

      len = 24;
      memset (packet, 0, len);
      //packet[0] = 0x82;
      error = receive(cfg, packet, len, 1000);
      if (error < 0) {
	      if ( cfg->debug ) warnx("receive failed");
	      return -1;
      }

      //fprintf(stderr, "length=%d\n", error);
      for (i = 0; i < len; i++)
      {
	      data[i] = packet[i];
      }

      return error;
}

// Does some mysterious conversions (for voltage in, for example).
uint8_t convert_data(double x)
{
	double rpot = (double)0.8 * CT_R1 / (x - (double)0.8) - CT_R2;
	double result = (257 * (rpot-CT_RW) / CT_RP);

	if (result<0) result = 0;
	if (result>255) result = 255;

	return (uint8_t)result;
}

// TODO This is not throughoutly tested! Beware of hardware roasting!
int dcdc_set_voltage(struct dcdc_cfg *cfg, double vout)
{
	uint8_t vout_raw = convert_data(vout);

	if (cfg->debug) warnx("Setting voltage byte to %d",vout_raw);

	int ret = send_command(cfg, CMD_WRITE_VOUT, vout_raw);
	if (ret < 0) printf("raw error: %d\n",ret);
	return DCDC_SUCCESS;
}
