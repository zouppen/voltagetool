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

int send (struct dcdc_cfg *cfg, unsigned char *data, int size)
{
	if (data == NULL)
		return -1;

	int error = usb_interrupt_write (cfg->dev, USB_ENDPOINT_OUT + 1,
					 (char *) data, size, 1000);
	if( error != size && cfg->debug )
		warnx("error on usb_interrupt_write, returned code %d", error);

	return error;
}

int receive (struct dcdc_cfg *cfg, unsigned char *data, int length, int timeout)
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

/*
    ros::NodeHandle handle;
    int debug_level;
    volatile bool stopRequest;

    int mode;
    float input_voltage;
    float output_voltage;

    void run()
    {
      std::stringstream ss;

      //
      //  Need to make the queue size big enough that each thread can publish without
      //  concern that one message it quickly replaced by another threads message.
      //
      ros::Publisher ps  = handle.advertise<minibox_dcdc::MiniboxDcDc>("minibox_dcdc", 5);

      ros::Rate rate(SAMPLE_RATE);            //set the rate we scan the device for input

      minibox_dcdc::MiniboxDcDc  powerState;
      uint8_t data[64];
      int error;

      while(handle.ok() && (stopRequest == false))
      {
        rate.sleep();
        ros::spinOnce();
#if 0
        count--;
        if(count == 0)
        {
          ROS_INFO("set output=%d", power);
          //error = send_command(CMD_INC_VOUT, power);
          //error = send_command(CMD_SET_AUX_WIN, power);
          //error = send_command(CMD_SET_PW_SWITCH, power);
          error = send_command(CMD_SET_OUTPUT, power);
          if (error < 0)
            ROS_ERROR ("Failed send_command");
          power ^= 1;
          count = reload;
        }
        else
#endif
        {
          error = get_all_values ();
          if (error < 0)
          {
            ROS_WARN ("get_all_values failed");
          }
        }

        error = read_status (data);
        if (error < 0)
          ROS_ERROR ("Failed read_status");
        else
        {

          switch(data[0])
          {
            case DCDCUSB_RECV_ALL_VALUES:
            {
              mode = data[1];

              //stat.add("mode", mode & 0x3);
              //stat.add("time config", (mode >> 5) & 0x7);
              //stat.add("voltage config", (mode >> 2) & 0x7);

              //stat.add("state", (int)data[2]); //7=good, 18= 1 minute to off
              powerState.state = data[2];
              //printf("state=%02x\n", data[2]);

              input_voltage = ((float)data[3] * 0.1558f);
              //stat.add("input voltage", input_voltage);
              powerState.input_voltage = input_voltage;

              float voltage = (float)data[4] * 0.1558f;
              //stat.add("ignition voltage", voltage);

              output_voltage = (float)data[5] * 0.1170f;
              //stat.add("output voltage", output_voltage);
              powerState.output_voltage = output_voltage;

              int status = data[6];
              //stat.add("Power Switch", ((status & 0x4) ? "True":"False"));
              powerState.power_switch = (status & 0x4);
              //stat.add("Output Enable", ((status & 0x8) ? "True":"False"));
              powerState.output_enable = (status & 0x8);
              //stat.add("AuxVin Enable", ((status & 0x10) ? "True":"False"));
              powerState.aux_enable = (status & 0x10);

              //ss.str("");
              //ss << "version " << ((data[23] >> 5) & 0x07) << "." << (data[23] & 0x1F);
				      //stat.add("version", ss.str());

              //msg_out.status.push_back(stat);

              ROS_DEBUG("mode=%x input_voltage=%.2f ignition_voltage=%.2f output_voltage=%.2f", 
                mode, input_voltage, voltage, output_voltage );
            }
            break;
            case INTERNAL_MESG:
            {
            }
            break;
            case DCDCUSB_CMD_IN:
            {
              ROS_DEBUG("received CMD_IN");
              //fprintf(stderr,"%x %x %x %x\n", data[0], data[1], data[2], data[3]);

              double rpot = ((double)data[3]) * CT_RP / (double)257 + CT_RW;
              double voltage = (double)80 * ( (double)1 + CT_R1/(rpot+CT_R2));
              voltage = floor(voltage);
              voltage = voltage/100;
              //stat.add("voltage", voltage);
              ROS_DEBUG("vout=%f", voltage);
              //msg_out.status.push_back(stat);
            }
            break;
            case DCDCUSB_MEM_READ_IN:
            {
            }
            break;

            default:
              ROS_WARN ("un-recognized message type = %x", data[0]);
          }

          powerState.header.stamp = ros::Time::now();
          ps.publish(powerState);
        }
      }
    }

};

int main(int argc, char** argv)
{
  int debug_level;

  po::options_description desc("Allowed options");
  desc.add_options()
    ("help", "this help message")
    ("debug", po::value<int>(&debug_level)->default_value(0), "debug level");

  po::variables_map vm;
  po::store(po::parse_command_line( argc, argv, desc), vm);
  po::notify(vm);

  if( vm.count("help"))
  {
    cout << desc << "\n";
    return 1;
  }

  ros::init(argc, argv, "minibox_dcdc");
  ros::NodeHandle handle;
  ros::NodeHandle private_handle("~");

  ROSCONSOLE_AUTOINIT;
  log4cxx::LoggerPtr my_logger = log4cxx::Logger::getLogger(ROSCONSOLE_DEFAULT_NAME);
  ROS_INFO("Logger Name: %s\n", ROSCONSOLE_DEFAULT_NAME);

  if( my_logger->getLevel() == 0 )    //has anyone set our level??
  {
    // Set the ROS logger
    my_logger->setLevel(ros::console::g_level_lookup[ros::console::levels::Info]);
  }

  private_handle.getParam( "debug_level", debug_level );
  ROS_DEBUG("debug_level=%d", debug_level);

  server server_list(debug_level);

  server_list.start();

  ros::spin(); //wait for ros to shut us down

  server_list.stop();
}
*/

int main(int argc, char **argv) {

	struct dcdc_cfg cfg;
	int ret = dcdc_init(&cfg, 1);

	if (ret!=DCDC_SUCCESS) {
		printf("hajosi");
		return 1;
	}

	dcdc_stop(&cfg);
	return 0;
}
