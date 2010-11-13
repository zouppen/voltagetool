#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <usb.h>

static const float SAMPLE_RATE = 5.0; // in Hz

#define STATUS_OK			0x00
#define STATUS_ERASE			0x01
#define STATUS_WRITE			0x02
#define STATUS_READ			0x03
#define STATUS_ERROR			0xff

#define DCDCUSB_GET_ALL_VALUES		0x81
#define DCDCUSB_RECV_ALL_VALUES		0x82
#define DCDCUSB_CMD_OUT			0xB1
#define DCDCUSB_CMD_IN			0xB2
#define DCDCUSB_MEM_READ_OUT		0xA1
#define DCDCUSB_MEM_READ_IN		0xA2
#define DCDCUSB_MEM_WRITE_OUT		0xA3
#define DCDCUSB_MEM_WRITE_IN		0xA4
#define DCDCUSB_MEM_ERASE		0xA5

#define INTERNAL_MESG			0xFF
#define INTERNAL_MESG_DISCONNECTED	0x01

#define CMD_SET_AUX_WIN			0x01
#define CMD_SET_PW_SWITCH		0x02
#define CMD_SET_OUTPUT			0x03
#define CMD_WRITE_VOUT			0x06
#define CMD_READ_VOUT			0x07
#define CMD_INC_VOUT			0x0C
#define CMD_DEC_VOUT			0x0D
#define CMD_LOAD_DEFAULTS		0x0E
#define CMD_SCRIPT_START		0x10
#define CMD_SCRIPT_STOP			0x11
#define CMD_SLEEP			0x12

//For reading out memory
#define TYPE_CODE_MEMORY		0x00
#define TYPE_EPROM_EXTERNAL		0x01
#define TYPE_EPROM_INTERNAL		0x02
#define TYPE_CODE_SPLASH		0x03

// AddressLo : AddressHi : AddressUp
// (anywhere inside the 64 byte-block to be erased)
#define FLASH_REPORT_ERASE_MEMORY	0xF2
// AddressLo : AddressHi : AddressUp : Data Length (1...32)
#define FLASH_REPORT_READ_MEMORY	0xF3
// AddressLo : AddressHi : AddressUp : Data Length (1...32) : Data....
#define FLASH_REPORT_WRITE_MEMORY	0xF4
// same as F2 but in keyboard mode
#define KEYBD_REPORT_ERASE_MEMORY	0xB2
// same as F3 but in keyboard mode
#define KEYBD_REPORT_READ_MEMORY	0xB3
// same as F4 but in keyboard mode
#define KEYBD_REPORT_WRITE_MEMORY	0xB4
// response to b3,b4
#define KEYBD_REPORT_MEMORY		0x41

#define IN_REPORT_EXT_EE_DATA		0x31
#define OUT_REPORT_EXT_EE_READ		0xA1
#define OUT_REPORT_EXT_EE_WRITE		0xA2

#define IN_REPORT_INT_EE_DATA		0x32
#define OUT_REPORT_INT_EE_READ		0xA3
#define OUT_REPORT_INT_EE_WRITE		0xA4

///// MEASUREMENT CONSTANTS
#define CT_RW	(double)75
#define CT_R1	(double)49900
#define CT_R2	(double)1500
#define CT_RP	(double)10000

#define CHECK_CHAR (unsigned char)0xAA //used for line/write check 

#define MAX_MESSAGE_CNT 64

#define DCDC_SUCCESS 0
#define DCDC_NO_DEVICE 1
#define DCDC_DETACH_ERROR 2
#define DCDC_DUMP_ERROR 3

struct dcdc_cfg {
	int debug;
	int stop_request;
	usb_dev_handle *dev;
};


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

int dcdc_debugdump(struct dcdc_cfg *cfg)
{
      uint8_t data[64];
      int error;

      error = get_all_values(cfg);
      if (error < 0) {
	      if (cfg->debug) warnx("get_all_values failed");
      }

      error = read_status(cfg,data);
      if (error < 0) {
	      if (cfg->debug) printf("Failed read_status");
	      return DCDC_DUMP_ERROR;
      }

      if(data[0] == DCDCUSB_RECV_ALL_VALUES) {
	      
              int mode = data[1];

	      printf("Mode: %d\n", mode & 0x3);
	      printf("Time config: %d\n", (mode >> 5) & 0x7);
	      printf("Voltage config: %d\n", (mode >> 2) & 0x7);
	      printf("State: %d\n", (int)data[2]); //7=good, 18= 1 minute to off

              double input_voltage = (double)data[3] * 0.1558f;
              double ignition_voltage = (double)data[4] * 0.1558f;
              double output_voltage = (double)data[5] * 0.1170f;

	      printf("Input voltage: %f\n", input_voltage);
	      printf("Ignition voltage: %f\n", ignition_voltage);
	      printf("Output voltage: %f\n", output_voltage);

              int status = data[6];

	      int power_switch = (status & 0x4);
	      int output_enable = (status & 0x8);
	      int auxvin_enable = (status & 0x10);

	      printf("Power switch: %d\n", power_switch);
	      printf("Output enable: %d\n", output_enable);
	      printf("Aux V in enable: %d\n", auxvin_enable);	      
      } else {
	      errx(1,"weird packet: %d", data[0]);
      }

      return DCDC_SUCCESS;
}

int main(int argc, char **argv) {

	struct dcdc_cfg cfg;
	int ret = dcdc_init(&cfg, 1);

	if (ret!=DCDC_SUCCESS) {
		printf("hajosi");
		return 1;
	}

	dcdc_debugdump(&cfg);

	dcdc_stop(&cfg);
	return 0;
}
