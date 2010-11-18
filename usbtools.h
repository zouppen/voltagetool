// TODO documentation

struct dcdc_cfg {
	int debug;
	int stop_request;
	usb_dev_handle *dev;
};

int dcdc_init(struct dcdc_cfg *cfg, int debug);
void dcdc_stop(struct dcdc_cfg *cfg);
int send(struct dcdc_cfg *cfg, unsigned char *data, int size);
int receive(struct dcdc_cfg *cfg, unsigned char *data, int length, int timeout);
int get_all_values(struct dcdc_cfg *cfg);
int send_command(struct dcdc_cfg *cfg, uint8_t command, uint8_t value);
int read_status(struct dcdc_cfg *cfg, uint8_t * data);
uint8_t convert_data(double x);
int dcdc_set_voltage(struct dcdc_cfg *cfg, double vout);

// Some constants

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
