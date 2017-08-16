#include <stdio.h>
#include <stdlib.h>
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

#include <at30ts74.h>
#include <mma7660.h>
#include <periph/gpio.h>
#include <periph/i2c.h>
#include "cmsis/samd21/include/instance/adc.h"
//#include "cmsis/samd21/include/component/adc.h"

// 1 second, defined in us
#define INTERVAL (1000000U)
#define SECOND (1000000U)
//#define INTERVAL (10000000U)

#define BOOST_ENABLE       GPIO_PIN(PA, 8)
#define I2C_RESET          GPIO_PIN(PA, 18)
#define LOW_BATT_INDICATOR GPIO_PIN(PA, 19)
#define FIELD_POWER_LED    GPIO_PIN(PA, 28)
#define D25    GPIO_PIN(PA, 25)
#define D27    GPIO_PIN(PA, 27)
#define D28    GPIO_PIN(PA, 28)

#define PE_ADDR 24
#define PE_IN_REG 0x0
#define PE_OUT_REG 0x1
#define PE_POL_REG 0x2
#define PE_CFG_REG 0x3

// Insole points
//#define JP1 (0x1)
#define JP1 (0x1 << 1)
#define JP2 (0x1 << 2)
#define JP3 (0x1 << 3)
#define JP4 (0x1 << 4)
#define JP5 (0x1 << 5)
#define JP6 (0x1 << 6)
#define JP7 (0x1 << 6)

extern int _netif_config(int argc, char **argv);
extern void send(char *addr_str, char *port_str, char *data, uint16_t datalen);
extern void start_server(char *port_str);
#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];
extern void handle_input_line(const shell_command_t *command_list, char *line);

uint16_t field_adc_val;

int active = 1;

void set_led(int led_state)
{
    printf("ACTIVE? %d LED %d\n", active, led_state);
    if (active)
    {
        int rv;
        if (i2c_acquire(I2C_0)) {
            printf("I2C acquire fail\n");
        }
        rv = i2c_write_reg(I2C_0, PE_ADDR, PE_OUT_REG, led_state);
        printf("Wrote %d bytes (%x)\n", rv, led_state);
        if (i2c_release(I2C_0)) {
            printf("I2C release fail\n");
        }
    }
}

// thread for listening for radio commands
char radio_stack[THREAD_STACKSIZE_MAIN];
void *radio_thread(void *arg)
{
  // message references
  msg_t msg;
  msg_t msg_queue[16];
  msg_init_queue(msg_queue, 16);
  gnrc_netreg_entry_t server = GNRC_NETREG_ENTRY_INIT_PID(GNRC_NETREG_DEMUX_CTX_ALL, thread_getpid());
  server.demux_ctx = (uint32_t)64833;
  gnrc_netreg_register(GNRC_NETTYPE_UDP, &server);
  //gnrc_netreg_entry_t entry = { NULL, 64833, thread_getpid() };
  //if (gnrc_netreg_register(GNRC_NETTYPE_UDP, &entry)) {
  //  printf("ERROR listening\n");
  //}

  // turn radio on
  kernel_pid_t radio[GNRC_NETIF_NUMOF];
  uint8_t radio_num = gnrc_netif_get(radio);
  netopt_state_t radio_state = NETOPT_STATE_IDLE;
  for (int i=0; i < radio_num; i++)
    gnrc_netapi_set(radio[i], NETOPT_STATE, 0, &radio_state, sizeof(netopt_state_t));

  // listen for commands
  while (1) {
    msg_receive(&msg);
    switch(msg.type)
    {
      case GNRC_NETAPI_MSG_TYPE_RCV:
      {
          printf("Received a packet \n");
          active = 1 - active; // toggle flag
      }
    }
  }

}

// method to read state of the device and report it out
char monitoring_stack[THREAD_STACKSIZE_MAIN];
void *monitoring(void *arg)
{
  // var declarations for monitoring
  int low_battery_indicator = 0;
  int8_t acc_x,acc_y,acc_z;
  int8_t old_acc_x = 0,old_acc_y = 0,old_acc_z = 0;
  int32_t temperature;

  int rv;
  int sleep = SECOND; // 1 second
  int inactive_time = 0; // how long we've been inactive
  int inactive_thresh = 300 * SECOND; // how long to wait before disabling

  // initialize sensors
  // temperature sensor
  at30ts74_t tmp;
  at30ts74_init(&tmp, I2C_0, AT30TS74_ADDR, AT30TS74_12BIT);
  // accelerometer
  mma7660_t acc;
  mma7660_params_t  mma7660_params = {
    .i2c = I2C_0,
    .addr = MMA7660_ADDR
  };
  if (mma7660_init(&acc, &mma7660_params) != 0) {
    printf("Failed to init ACC\n");
  } else {
    printf("Init acc ok\n");
  }
  if (mma7660_set_mode(&acc, 1, 0, 0, 0) != 0) {
    printf("Failed to set mode\n");
  } else {
    printf("Set mode ok\n");
  }
  if (mma7660_config_samplerate(&acc, MMA7660_SR_AM64, MMA7660_SR_AW32, 1) != 0) {
    printf("Failed to config SR\n");
  }

  // initialize timer
  xtimer_ticks32_t last_wakeup = xtimer_now();
  while (1) {
        // read temperature sensor
        rv = at30ts74_read(&tmp, &temperature);
        if (rv != 0 ) {
            printf("Failed to read temp sensor (%d)\n", rv);
        }

        // read accelerometer
        rv = mma7660_read_counts(&acc, &acc_x, &acc_y, &acc_z);
        if (rv != 0 ) {
            printf("Failed to read accelerometer (%d)\n", rv);
        }

        printf("STATUS:\n");
        printf(" LBI: %d\n", low_battery_indicator);
        printf(" Temperature %" PRId32 "\n", temperature);
        printf(" Accel x/y/z %d/%d/%d\n",acc_x, acc_y, acc_z);
        printf("-------\n");

        //if (i2c_acquire(I2C_0)) {
        //    printf("I2C acquire fail\n");
        //}

        int8_t diff = abs(old_acc_x - acc_x);
        diff += abs(old_acc_y - acc_y);
        diff += abs(old_acc_z - acc_z);

        if(diff > 10){
            // turn on!
            gpio_write(BOOST_ENABLE, 1);
            active = 1;
            inactive_time = 0;
            // want to disable if idle. This is on a really long timer
            printf("Acc diff %d\n", diff);
        } else if (active) {
            // if active is ON, but we are not moving, then start the timer
            inactive_time += sleep;
        }
        printf("Acc inactive %d\n", inactive_time);

        if (inactive_time > inactive_thresh)
        {
            printf("Acc disabling");
            active = 0;
            if (i2c_acquire(I2C_0)) {
                printf("I2C acquire fail\n");
            }
            rv = i2c_write_reg(I2C_0, PE_ADDR, PE_OUT_REG, 0);
            printf("Wrote %d bytes (%x)\n", rv, 0);
            if (i2c_release(I2C_0)) {
                printf("I2C release fail\n");
            }
            gpio_write(BOOST_ENABLE, 0);
        }

        old_acc_x = acc_x;
        old_acc_y = acc_y;
        old_acc_z = acc_z;

        xtimer_periodic_wakeup(&last_wakeup, sleep);
  }
}

// method to read state of the device and report it out
void cycle_all(void)
{
  int waketime = 20*SECOND;

  // initialize timer
  xtimer_ticks32_t last_wakeup = xtimer_now();
  while (1) {
        set_led(JP1);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP2);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP3);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP4);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP5);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP6);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
  }
}

void cycle_pairs(void)
{
  int waketime = 2*SECOND;

  // initialize timer
  xtimer_ticks32_t last_wakeup = xtimer_now();
  while (1) {
        set_led(JP1 | JP2);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP3 | JP4);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP5 | JP6);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
  }
}

void cycle_pairs4(void)
{
  int waketime = 4*SECOND;

  // initialize timer
  xtimer_ticks32_t last_wakeup = xtimer_now();
  while (1) {
        set_led(JP1);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP2);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP3);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        set_led(JP4);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
  }
}


void dummy(void)
{
  xtimer_ticks32_t last_wakeup = xtimer_now();
  int waketime = 1000000;
  uint8_t led_state = JP1 | JP2 | JP3 | JP4 | JP5 | JP6;
  int rv = i2c_write_reg(I2C_0, PE_ADDR, PE_OUT_REG, led_state);
  printf("Wrote %d bytes (%x)\n", rv, led_state);
  if (i2c_release(I2C_0)) {
      printf("I2C release fail\n");
  }
  while (1)
  {
    xtimer_periodic_wakeup(&last_wakeup, waketime);
    printf("LBO %d\n", gpio_read(LOW_BATT_INDICATOR));
  }
}

char read_adc_stack[THREAD_STACKSIZE_MAIN];
void *read_adc_thread(void *arg)
{
    // default config?
    REG_PM_APBCMASK |= PM_APBCMASK_ADC;
    REG_GCLK_CLKCTRL=(GCLK_CLKCTRL_CLKEN)|(GCLK_CLKCTRL_GEN_GCLK2)|(GCLK_CLKCTRL_ID_ADC);//4MHz

    // disable ADC module
    printf("disable ADC module\n");
    REG_ADC_CTRLA &= ~(ADC_CTRLA_ENABLE);
    while (REG_ADC_STATUS & ADC_STATUS_SYNCBUSY);

    // software reset
    printf("software reset ADC module\n");
    REG_ADC_CTRLA = ADC_CTRLA_SWRST;
    while ((REG_ADC_STATUS & ADC_STATUS_SYNCBUSY) || (REG_ADC_CTRLA & ADC_CTRLA_SWRST));

    // ADC input setting, AIN06 (PA06?)
    printf("configure adc input\n");
    PORT->Group[0].PINCFG[6].reg = PORT_PINCFG_PMUXEN; //PA6, pin7
    PORT->Group[0].PMUX[3].reg &= ~(PORT_PMUX_PMUXE_Msk);
    PORT->Group[0].PMUX[3].reg |= PORT_PMUX_PMUXE_B; //ADC AIN[6]

    REG_ADC_INPUTCTRL = (ADC_INPUTCTRL_MUXPOS_PIN6) | (ADC_INPUTCTRL_MUXNEG_IOGND);
    while (REG_ADC_STATUS & ADC_STATUS_SYNCBUSY);

    // V_REF setting
    REG_ADC_REFCTRL = ADC_REFCTRL_REFSEL_INT1V;

    // set average control
    REG_ADC_AVGCTRL = ADC_AVGCTRL_SAMPLENUM_512;

    // clear interrupt flag
    REG_ADC_INTFLAG = ADC_INTFLAG_RESRDY;

    // enable freerun
    REG_ADC_CTRLB = ADC_CTRLB_PRESCALER_DIV8 | ADC_CTRLB_RESSEL_16BIT | ADC_CTRLB_FREERUN;

    // enable ADC module
    printf("ENABLE ADC module\n");
    REG_ADC_CTRLA |= (ADC_CTRLA_ENABLE);
    while (REG_ADC_STATUS & ADC_STATUS_SYNCBUSY);

    printf("watching intflag\n");

    xtimer_ticks32_t last_wakeup = xtimer_now();
    int waketime = 1.5*SECOND;
    while (1)
    {
        while (1)
        {
            // value ready
            if (REG_ADC_INTFLAG & ADC_INTFLAG_RESRDY)
            {
                while (REG_ADC_STATUS & ADC_STATUS_SYNCBUSY); // wait for sync
                // read value
                field_adc_val = REG_ADC_RESULT;
                printf("val %d\n", field_adc_val);
                gpio_write(D25, field_adc_val > 10000);
                gpio_write(D27, field_adc_val > 10000);
                gpio_write(D28, field_adc_val > 10000);
                break;
            }
        }
        //printf("%x %x\n", REG_ADC_STATUS, REG_ADC_INTFLAG);
        xtimer_periodic_wakeup(&last_wakeup, waketime);
        //printf("%lu\n", REG_ADC_INTFLAG & ADC_INTFLAG_RESRDY);
    }
    // remember to:
    // - make debug
    // - continue
    // - delete breakpoints
    // - jump main

}

void low_batt_trig(void *arg) {
    printf("LOW BATTERY!\n");
}


int main(void)
{
    int rv, state;
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    printf("Started!\n");

    rv = gpio_init(BOOST_ENABLE, GPIO_OUT);
    if (rv != 0) {
        printf("Could not init PA08 as output (%d)\n", rv);
        return 1;
    }
    printf("Initialized PA08 as OUTPUT\n");
    rv = gpio_read(BOOST_ENABLE);
    printf("PA08 state: %d\n", rv);
    gpio_write(BOOST_ENABLE, 1);
    printf("Enabled BOOST chip");

    rv = gpio_init(I2C_RESET, GPIO_OUT);
    if (rv != 0) {
        printf("Could not init PA18 as output (%d)\n", rv);
        return 1;
    }
    printf("Initialized PA18 as OUTPUT\n");
    gpio_write(I2C_RESET, 1);
    state = gpio_read(I2C_RESET);
    printf("PA18 state: %d\n", state);

    rv = gpio_init_int(LOW_BATT_INDICATOR, GPIO_IN_PU, GPIO_FALLING, low_batt_trig, 0);
    if (rv != 0) {
        printf("Could not init PA19 as input (%d)\n", rv);
        return 1;
    }

    // initialize I2C port expander
    if (i2c_acquire(I2C_0)) {
        printf("I2C acquire fail\n");
    }
    if (i2c_init_master(I2C_0, I2C_SPEED_NORMAL)) {
        printf("I2C init fail\n");
    }

    // set the OUTPUT setting to 0
    rv = i2c_write_reg(I2C_0, PE_ADDR, PE_OUT_REG, 0x00);
    printf("Wrote %d bytes (%x)\n", rv, 0x00);
    // configure all pins as OUTPUT
    rv = i2c_write_reg(I2C_0, PE_ADDR, PE_CFG_REG, 0x00);
    printf("Wrote %d bytes \n", rv);

    if (i2c_release(I2C_0)) {
        printf("I2C release fail\n");
    }

    rv = gpio_init(FIELD_POWER_LED, GPIO_OUT);
    if (rv != 0) {
        printf("Could not init PA28 as output (%d)\n", rv);
        return 1;
    }
    printf("Initialized PA28 as OUTPUT\n");
    gpio_write(FIELD_POWER_LED, 1);

    gpio_init(D25, GPIO_OUT);
    gpio_init(D27, GPIO_OUT);
    gpio_init(D28, GPIO_OUT);
    gpio_write(D25, 0);
    gpio_write(D27, 0);
    gpio_write(D28, 0);

    // start thread to do ADC measurements
    thread_create(read_adc_stack, sizeof(read_adc_stack),
                  THREAD_PRIORITY_MAIN-1, THREAD_CREATE_STACKTEST,
                  read_adc_thread, NULL, "read_adc_thread");

    // start thread to listen for radio commands
    //thread_create(radio_stack, sizeof(radio_stack),
    //              THREAD_PRIORITY_MAIN+2, THREAD_CREATE_STACKTEST,
    //              radio_thread, NULL, "radio_thread");

    // TODO: put monitoring in one thread, put the actuation in another thread
        //rv = i2c_write_reg(I2C_0, PE_ADDR, PE_OUT_REG, JP6);
    thread_create(monitoring_stack, sizeof(monitoring_stack),
                THREAD_PRIORITY_MAIN+3, THREAD_CREATE_STACKTEST,
                monitoring, NULL, "monitoring");
    //monitoring();
    //cycle_all();
    //cycle_pairs();
    cycle_pairs4();
    //cycle_pairs4();
    //cycle_single();
    //dummy();
    //read_adc();

    return 0;
}
