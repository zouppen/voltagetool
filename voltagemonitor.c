#include <stdio.h>
#include <usb.h>
#include <math.h>
#include <sys/time.h>

#include "usbtools.h"

double dcdc_get_voltage(struct dcdc_cfg *cfg);
long long get_microsecs();

long long get_microsecs() {
	struct timeval tv;

	if (gettimeofday(&tv, NULL))
		err("Error while accessing system time");
	
	return (long long)tv.tv_sec * 1e6 + tv.tv_usec;
}


double dcdc_get_voltage(struct dcdc_cfg *cfg)
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

              double input_voltage = (double)data[3] * 0.1558f;
	      return input_voltage;

      } else {
	      errx(1,"weird packet: %d", data[0]);
	      return NAN;
      }
}


int main(int argc, char **argv)
{
	// Selecting time interval
	double interval_user = 1;

	if (argc > 2) {
		errx(1,"Too many arguments. Usage: %s [interval]", argv[0]);
	} else if (argc == 2) {
		if (sscanf(argv[1], "%lf", &interval_user) != 1)
			errx(2,"Invalid interval. Not a number!");
	}

	fprintf(stderr, "Monitoring voltage every %lf seconds.\n",
		interval_user);

	int interval = interval_user * 1e6;

	struct dcdc_cfg cfg;
	int ret = dcdc_init(&cfg, 1);

	if (ret!=DCDC_SUCCESS) {
		return 1;
	}

	printf("microseconds,voltage in\n");

	while (1) {
		long long before_sleep = get_microsecs();
		long long wait = interval - (before_sleep % interval);
		
		if (usleep(wait))
			err("Failed to wait.");

		long long after_sleep = get_microsecs();
		double vin = dcdc_get_voltage(&cfg);
		
		printf("%lld,%lf\n",after_sleep, vin);
	}

	dcdc_stop(&cfg);
	return 0;
}
