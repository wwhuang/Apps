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
#include "net/gnrc/netdev2.h"
#include "net/gnrc/netreg.h"
#include <at30ts74.h>
#include <mma7660.h>
#include <periph/gpio.h>

// 1 second, defined in us
#define OFF_INTERVAL (1000000UL)

void cb(void* arg);

xtimer_t * timer;
at30ts74_t tmp;
mma7660_t acc;
kernel_pid_t radio[GNRC_NETIF_NUMOF];
uint8_t radio_num;


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

	// Radio set
	radio_num = gnrc_netif_get(radio);

}

void cb(void* arg) {

	int32_t temp = 0;
	int8_t x = 0;
	int8_t y = 0;
	int8_t z = 0;
	uint8_t i = 0;

	printf("[%lu] Sensing starts\n",  xtimer_usec_from_ticks(xtimer_now()));

	at30ts74_read(&tmp, &temp);

    if (mma7660_set_mode(&acc, 1, 0, 0, 0) != 0)
    	printf("Failed to set active mode\n");
    if (mma7660_read(&acc, &x, &y, &z))
		printf("Faile to read accel\n");
    if (mma7660_set_mode(&acc, 0, 0, 0, 0) != 0)
    	printf("Failed to set idle mode\n");

	printf("[%lu] temperature: %luC / accel %d %d %d\n", xtimer_usec_from_ticks(xtimer_now()),
			temp, x, y, z);

	netopt_state_t radio_state = NETOPT_STATE_IDLE;
	for (i=0; i < radio_num; i++)
		gnrc_netapi_set(radio[i], NETOPT_STATE, 0, &radio_state, sizeof(netopt_state_t));
	radio_state = NETOPT_STATE_SLEEP;
	for (i=0; i < radio_num; i++)
		gnrc_netapi_set(radio[i], NETOPT_STATE, 0, &radio_state, sizeof(netopt_state_t));

}

int main(void)
{
    low_power_init();

	while (1) {
		cb(0);
		xtimer_usleep(OFF_INTERVAL);
	}

    return 0;
}
