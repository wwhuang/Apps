#include <stdio.h>
#include <rtt_stdio.h>
#include "thread.h"
#include "xtimer.h"
#include <string.h>

#include "msg.h"
#include "net/gnrc.h"
#include "net/gnrc/ipv6.h"
#include "net/gnrc/udp.h"
#include "net/gnrc/netapi.h"
#include "net/gnrc/netreg.h"

#include <periph/gpio.h>
#include <periph/i2c.h>
#include <periph/adc.h>

#define SAMPLE_INTERVAL ( 10000000UL)
#define SAMPLE_JITTER   ( 2000000UL)

#define MAG_ACC_TYPE_FIELD 5
#define TEMP_TYPE_FIELD 6

void send_udp(char *addr_str, uint16_t port, uint8_t *data, uint16_t datalen);

void critical_error(void) {
  printf("CRITICAL ERROR, REBOOT\n");
  return;
  NVIC_SystemReset();
}

void low_power_init(void) {
    // Light sensor off
    gpio_init(GPIO_PIN(0,28), GPIO_OUT);
    gpio_init(GPIO_PIN(0,19), GPIO_OUT);
    gpio_write(GPIO_PIN(0, 28), 1);
    gpio_write(GPIO_PIN(0, 19), 0);
}

uint32_t interval_with_jitter(void)
{
    int32_t t = SAMPLE_INTERVAL;
    t += rand() % SAMPLE_JITTER;
    t -= SAMPLE_JITTER / 2;
    return (uint32_t)t;
}



int main(void)
{
    netopt_state_t radio_state = NETOPT_STATE_SLEEP;

    //This value is good randomness and unique per mote
    low_power_init();
    kernel_pid_t radio[GNRC_NETIF_NUMOF];
    uint8_t radio_num = gnrc_netif_get(radio);
    while (1) {
      //Send
      char *msg = "helloworld ";
      send_udp("ff02::1",6060,(uint8_t*)&msg,10);
      //Radio off
      for (int i=0; i < radio_num; i++)
        gnrc_netapi_set(radio[i], NETOPT_STATE, 0, &radio_state, sizeof(netopt_state_t));
      //Sleep
      xtimer_usleep(interval_with_jitter());
    }

    return 0;
}
