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

#include <hdc1000.h>
#include <mma7660.h>
#include <periph/gpio.h>
#include <periph/i2c.h>
#include "cmsis/samd21/include/instance/adc.h"

#define SECOND (1000000U)

// temperature sensor
#define TEMP_HUMID_SENSOR 0x40

// LED actuation groups
// display setpoint
#define LED_DRIVER_SP 0x61
// displays temperature
#define LED_DRIVER_TEMP 0x63
#define LED_DRIVER_3 0x67
#define ALLCALLADR 0x68

// TLC59116 registers
#define TLC59116_MODE1 0x00
#define TLC59116_MODE2 0x01
#define TLC59116_PWM0 0x02
#define TLC59116_PWM1 0x03
#define TLC59116_PWM2 0x04
#define TLC59116_PWM3 0x05
#define TLC59116_PWM4 0x06
#define TLC59116_PWM5 0x07
#define TLC59116_PWM6 0x08
#define TLC59116_PWM7 0x09
#define TLC59116_PWM8 0x0a
#define TLC59116_PWM9 0x0b
#define TLC59116_PWM10 0x0c
#define TLC59116_PWM11 0x0d
#define TLC59116_PWM12 0x0e
#define TLC59116_PWM13 0x0f
#define TLC59116_PWM14 0x10
#define TLC59116_PWM15 0x11
#define TLC59116_GRPPWM 0x12
#define TLC59116_GRPFREQ 0x13
#define TLC59116_LEDOUT0 0x14
#define TLC59116_LEDOUT1 0x15
#define TLC59116_LEDOUT2 0x16
#define TLC59116_LEDOUT3 0x17
#define TLC59116_SUBADR1 0x18
#define TLC59116_SUBADR2 0x19
#define TLC59116_SUBADR3 0x1a
#define TLC59116_ALLCALLADR 0x1b
#define TLC59116_IREF 0x1c
#define TLC59116_EFLAG1 0x1d
#define TLC59116_EFLAG2 0x1e

// GPIO pins
#define D18    GPIO_PIN(PA, 18)
#define D19    GPIO_PIN(PA, 19)
#define D24    GPIO_PIN(PA, 24)
#define D25    GPIO_PIN(PA, 25)
#define SP_DEC    GPIO_PIN(PA, 18)
#define SP_INC    GPIO_PIN(PA, 19)
#define POWER    GPIO_PIN(PA, 24)
#define TIMER    GPIO_PIN(PA, 25)

// constants
#define TIMER_MAX (60*60*SECOND)
#define TIMER_INTERVAL (15*60*SECOND)

#define MAIN_QUEUE_SIZE     (8)
static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

// state variables
int power = 1;
int16_t temperature = 70;
int cooling_setpoint = 75;
int heating_setpoint = 70;
uint32_t timer = 0;

int temperature_led_mapping[16][2] = {
    {58, TLC59116_PWM0},
    {61, TLC59116_PWM1},
    {63, TLC59116_PWM2},
    {65, TLC59116_PWM3},
    {67, TLC59116_PWM4},
    {69, TLC59116_PWM5},
    {71, TLC59116_PWM6},
    {73, TLC59116_PWM7},
    {75, TLC59116_PWM8},
    {77, TLC59116_PWM9},
    {79, TLC59116_PWM10},
    {81, TLC59116_PWM11},
    {83, TLC59116_PWM12},
    {85, TLC59116_PWM13},
    {88, TLC59116_PWM14},
    {92, TLC59116_PWM15},
};

int timer_led_mapping[4][2] = {
    {(1*TIMER_INTERVAL), TLC59116_PWM0},
    {(2*TIMER_INTERVAL), TLC59116_PWM3},
    {(3*TIMER_INTERVAL), TLC59116_PWM2},
    {(4*TIMER_INTERVAL), TLC59116_PWM1},
};

void get_i2c_bus(void) {
    while (i2c_acquire(I2C_0)) {
        printf("I2C acquire fail\n");
    }
}

void release_i2c_bus(void) {
    while (i2c_release(I2C_0)) {
        printf("I2C release fail\n");
    }
}

// turn all LEDS of on the given driver
void led_off(int driver) {
    get_i2c_bus();
    for (int i=0;i<16;i++) {
        int reg = temperature_led_mapping[i][1];
        i2c_write_reg(I2C_0, driver, reg, 0x00);
    }
    release_i2c_bus();
}

/*
 * Given a temperature, floor it to the nearest temperature that we recognize on the UI
 */
void nearest_temperature(int *temp, int *display_temp, int *led_register) {
    int i=1;
    if (*temp <= 58) {
        *display_temp = temperature_led_mapping[0][0];
        *led_register = temperature_led_mapping[0][1];
        *temp = 58;
    } else if (*temp >= 92) {
        *display_temp = temperature_led_mapping[15][0];
        *led_register = temperature_led_mapping[15][1];
        *temp = 92;
    }

    for (;i<16;i++) {
        int _display_temp = temperature_led_mapping[i][0];

        if (_display_temp == *temp) {
            *display_temp = temperature_led_mapping[i][0];
            *led_register = temperature_led_mapping[i][1];
            return;
        }

        if (_display_temp > *temp) {
            *display_temp = temperature_led_mapping[i-1][0];
            *led_register = temperature_led_mapping[i-1][1];
            return;
        }
    }
}

/*
 * Given a temperature, highlight the 
 */
void update_setpoint_display(int diff) {
    int display_temp_heat, display_temp_cool, led_register_heat, led_register_cool;

    // adjust setpoints
    heating_setpoint += diff;
    cooling_setpoint += diff;

    // if temperature closer to heating setpoint
    nearest_temperature(&heating_setpoint, &display_temp_heat, &led_register_heat);
    nearest_temperature(&cooling_setpoint, &display_temp_cool, &led_register_cool);

    led_off(LED_DRIVER_SP);

    printf("Heating SP %d has nearest display temp %d\n", heating_setpoint, display_temp_heat);
    printf("Cooling SP %d has nearest display temp %d\n", cooling_setpoint, display_temp_cool);
    get_i2c_bus();
    i2c_write_reg(I2C_0, LED_DRIVER_SP, led_register_heat, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, led_register_cool, 0xff);
    release_i2c_bus();
}

void update_temperature_display(void) {
    int display_temp, led_register_temp;
    int t = (int)temperature;
    nearest_temperature(&t, &display_temp, &led_register_temp);

    led_off(LED_DRIVER_TEMP);

    printf("Temperature %d has nearest display temp %d\n", t, display_temp);
    get_i2c_bus();
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, led_register_temp, 0x0f);
    release_i2c_bus();
}

void update_timer(void) {
    led_off(LED_DRIVER_3);
    for (int i=0;i<4;i++)
    {
        if (timer >= timer_led_mapping[i][0]) {
            printf("turn timer %d\n", i);
            get_i2c_bus();
            i2c_write_reg(I2C_0, LED_DRIVER_3, timer_led_mapping[i][1], 0x0f);
            release_i2c_bus();
        }
    }
    //int num_leds = 
}

// returns true if the button was pressed; handles debounce
xtimer_ticks32_t last_press;
int valid_press(void) {
    xtimer_ticks32_t press = xtimer_now();
    xtimer_ticks32_t diff = xtimer_diff(press, last_press);
    if (xtimer_usec_from_ticks(diff) > 0.2*SECOND) {
        last_press = press;
        return 1;
    }
    return 0;
}

void button_press_setpoint_dec(void *arg) {
    if (valid_press()) {
        printf("DECREASE SETPOINT\n");
        update_setpoint_display(-2);
    }
}

void button_press_setpoint_inc(void *arg) {
    if (valid_press()) {
        printf("INCREASE SETPOINT\n");
        update_setpoint_display(2);
    }
}

void button_press_power(void *arg) {
    if (valid_press()) {
        printf("POWER\n");
    }
}

void button_press_timer(void *arg) {
    if (valid_press()) {
        printf("TIMER\n");
        if (timer == TIMER_MAX) {
            timer = 0;
            update_timer();
            return;
        }

        int _newtimer = timer + TIMER_INTERVAL;
        if (_newtimer > TIMER_MAX) {
            timer = TIMER_MAX;
        } else {
            timer = _newtimer;
        }
        printf("TIMER now %"PRIu32"\n", timer);
        update_timer();
    }
}

// method to read state of the device and report it out
char monitoring_stack[THREAD_STACKSIZE_MAIN];
void *monitoring(void *arg)
{
    xtimer_ticks32_t last_wakeup = xtimer_now();
    int waketime = 5 * SECOND;

    hdc1000_t dev;
    hdc1000_params_t hdc1000_params = {
        .i2c = I2C_0,
        .addr = TEMP_HUMID_SENSOR,
        .res = HDC1000_14BIT,
    };
    if (hdc1000_init(&dev, &hdc1000_params) != 0) {
        printf("Failed to init temp/humidity sensor\n");
    }


    while(1)
    {
        int16_t hum;
        int16_t temp;
        hdc1000_read(&dev, &temp, &hum);
        temperature = (1.8*temp+3200) / 100;
        printf("TEMPERATURE --> %" PRId16 "\n", temperature);
        printf("HUMIDITY --> %" PRId16 "\n", hum);
        update_temperature_display();
        xtimer_periodic_wakeup(&last_wakeup, waketime);
    }
}

int main(void)
{
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    printf("Started!\n");


    // initialize I2C port expander

    // get bus
    if (i2c_acquire(I2C_0)) {
        printf("I2C acquire fail\n");
    }
    if (i2c_init_master(I2C_0, I2C_SPEED_NORMAL)) {
        printf("I2C init fail\n");
    }

    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_MODE1, 0x01);
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_MODE2, 0x00);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_MODE1, 0x01);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_MODE2, 0x00);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_MODE1, 0x01);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_MODE2, 0x00);

    // 0xaa -> 0b10101010. Means each LED can be controlled through PWM register
    // 0xff -> 0b10101010. Means each LED can be controlled through PWM register AND responds to "group" commands
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_LEDOUT0, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_LEDOUT0, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_LEDOUT0, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_LEDOUT1, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_LEDOUT1, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_LEDOUT1, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_LEDOUT2, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_LEDOUT2, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_LEDOUT2, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_TEMP, TLC59116_LEDOUT3, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_SP, TLC59116_LEDOUT3, 0xff);
    i2c_write_reg(I2C_0, LED_DRIVER_3, TLC59116_LEDOUT3, 0xff);

    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM0, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM1, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM2, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM3, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM4, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM5, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM6, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM7, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM8, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM9, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM10, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM11, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM12, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM13, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM14, 0);
    i2c_write_reg(I2C_0, ALLCALLADR, TLC59116_PWM15, 0);
    if (i2c_release(I2C_0)) {
        printf("I2C release fail\n");
    }
    printf("initialized LED drivers\n");


	/*
     * Initialize Button Interrupts
     * Setpoint dec: J1 -- D18
     * Setpoint inc: J2 -- D19
     * power: J3 -- D24
     * timer: J4 -- D25
	 */
    gpio_init_int(SP_DEC, GPIO_IN_PU, GPIO_FALLING, (gpio_cb_t)button_press_setpoint_dec, 0);
    gpio_init_int(SP_INC, GPIO_IN_PU, GPIO_FALLING, (gpio_cb_t)button_press_setpoint_inc, 0);
    gpio_init_int(POWER, GPIO_IN_PU, GPIO_FALLING, (gpio_cb_t)button_press_power, 0);
    gpio_init_int(TIMER, GPIO_IN_PU, GPIO_FALLING, (gpio_cb_t)button_press_timer, 0);

    thread_create(monitoring_stack, sizeof(monitoring_stack),
                THREAD_PRIORITY_MAIN, THREAD_CREATE_STACKTEST,
                monitoring, NULL, "monitoring");
    update_setpoint_display(0);

    xtimer_ticks32_t last_wakeup = xtimer_now();
    while (1) {
        xtimer_periodic_wakeup(&last_wakeup, 100000);
    }

    return 0;
}
