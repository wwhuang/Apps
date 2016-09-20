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

void low_power_init(void) {

    // Light sensor off
    gpio_init(GPIO_PIN(0,28), GPIO_OUT);
    gpio_write(GPIO_PIN(0, 28), 1); 
    
    // Temperature sensor off
    at30ts74_t tmp;
    if (at30ts74_init(&tmp, I2C_0, AT30TS74_ADDR, AT30TS74_12BIT) != 0)
        printf("Failed to init TEMP\n");
    
    // Accelerometer off
    mma7660_t acc;
    if (mma7660_init(&acc, I2C_0, MMA7660_ADDR) != 0)
      printf("Failed to init ACC\n");
    if (mma7660_set_mode(&acc, 0, 0, 0, 0) != 0)
      printf("Failed to set idle mode\n");    
 
    printf("Sensors Off\n");
    rtt_init();   
    printf("RTT initialization (%u %u)\n", RTT_FREQUENCY, RTT_MAX_VALUE);
    
    // CPU idle (main clock is active)    
}

void cb(void* arg) {
	printf("call back\n");
    //clk_init();
	periodic_task(0);
}

void periodic_task(void* arg) {
	uint32_t now;
    uint32_t i =0; //uint32_t last_wakeup = xtimer_now();
	//printf("0\n");
    while (i < 1000) {
		i++;
	}
    //xtimer_periodic_wakeup(&last_wakeup, ON_INTERVAL);
	//lpm_end_awake();
	//printf("1\n");
	now = rtt_get_counter() + OFF_INTERVAL;
	//printf("2\n");
	now = (now > RTT_MAX_VALUE) ? now - RTT_MAX_VALUE : now;

	//printf("next wakeup: %lu\n", now);
    //printf("sleep mode %u\n", lpm_set(LPM_OFF));
    rtt_set_alarm(now, cb, 0);
    //lpm_set(LPM_OFF);   
}

int main(void)
{
    low_power_init();
    periodic_task(0);
    while(1) {
    	xtimer_usleep(1000000U);
    }
    return 0;
}
