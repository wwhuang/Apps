#include <stdio.h>
#include <rtt_stdio.h>
#include "shell.h"
#include "thread.h"
#include "xtimer.h"
#include <string.h>

#include "msg.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"

#include "arch/lpm_arch.h"
#include "periph/rtt.h"
#include <at30ts74.h>
#include <mma7660.h>
#include <periph/gpio.h>

// 1 second, defined in us
#define OFF_INTERVAL (RTT_FREQUENCY)
#define NETWORK_RTT_US 1000000
#define ON_INTERVAL (1000000UL)
void cb(void* arg);
void periodic_task(void* arg);
xtimer_t * timer;
at30ts74_t tmp;
mma7660_t acc;

void low_power_init(void) {

    // Light sensor off
    gpio_init(GPIO_PIN(0,28), GPIO_OUT);
    gpio_write(GPIO_PIN(0, 28), 1); 
    
    // Temperature sensor off
    if (at30ts74_init(&tmp, I2C_0, AT30TS74_ADDR, AT30TS74_12BIT) != 0)
        printf("Failed to init TEMP\n");
    
    // Accelerometer off
    if (mma7660_init(&acc, I2C_0, MMA7660_ADDR) != 0)
      printf("Failed to init ACC\n");
    if (mma7660_set_mode(&acc, 0, 0, 0, 0) != 0)
      printf("Failed to set idle mode\n");    
	if (mma7660_config_samplerate(&acc, MMA7660_SR_AM64, MMA7660_SR_AW32, 1) != 0)
      printf("Failed to config SR\n");
    

    printf("Sensors Off\n");
    rtt_init();   
    printf("RTT initialization (%u %u)\n", RTT_FREQUENCY, RTT_MAX_VALUE);    
}

void cb(void* arg) {
	int32_t temp = 0;
	int8_t x = 0;
	int8_t y = 0;
	int8_t z = 0;

	printf("[%lu] Sensing starts\n", rtt_get_counter());

	at30ts74_read(&tmp, &temp);

    /*if (mma7660_set_mode(&acc, 1, 0, 0, 0) != 0)
    	printf("Failed to set active mode\n"); 
    if (mma7660_read(&acc, &x, &y, &z))
		printf("Faile to read accel\n");    
    if (mma7660_set_mode(&acc, 0, 0, 0, 0) != 0)
    	printf("Failed to set idle mode\n"); */

	printf("[** temperature: %luC / accel %d %d %d **]\n",
			temp, x, y, z);

	periodic_task(0);
}

void periodic_task(void* arg) {
	uint32_t now;

	now = rtt_get_counter() + OFF_INTERVAL;
	now = (now > RTT_MAX_VALUE) ? now - RTT_MAX_VALUE : now;
	printf("[%lu] sleep\n\n", rtt_get_counter());
    rtt_set_alarm(now, cb, 0);
}

int main(void)
{
    low_power_init();
    periodic_task(0);

	while (1) {	
		xtimer_usleep(1000000UL);
	}

    return 0;
}
