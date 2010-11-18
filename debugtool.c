#include <stdio.h>
#include <usb.h>

#include "usbtools.h"


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


int main(int argc, char **argv)
{
	struct dcdc_cfg cfg;
	int ret = dcdc_init(&cfg, 1);

	if (ret!=DCDC_SUCCESS) {
		printf("hajosi");
		return 1;
	}
	
	// Experimental voltage selection if given from cmd line.
	if (argc==2) {
		double vout;
		if (sscanf(argv[1], "%lf", &vout) == 1) {
			printf("Setting voltage to %f volts.\n", vout);
			dcdc_set_voltage(&cfg,vout);
		} else {
			warnx("Invalid voltage");
		}
	}

	dcdc_debugdump(&cfg);

	dcdc_stop(&cfg);
	return 0;
}
