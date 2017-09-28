#include <stdio.h>
#include <rtt_stdio.h>
#include "xtimer.h"
#include <string.h>
#include "saul_reg.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

#ifndef SAMPLE_INTERVAL
#define SAMPLE_INTERVAL ( 1000000UL)
#endif

saul_reg_t *sensor_radtemp_t = NULL;
saul_reg_t *sensor_temp_t    = NULL;
saul_reg_t *sensor_hum_t     = NULL;
saul_reg_t *sensor_mag_t     = NULL;
saul_reg_t *sensor_accel_t   = NULL;
saul_reg_t *sensor_light_t   = NULL;
saul_reg_t *sensor_occup_t   = NULL;
saul_reg_t *sensor_button_t  = NULL;

void critical_error(void) {
    DEBUG("CRITICAL ERROR, REBOOT\n");
    NVIC_SystemReset();
    return;
}

void sensor_config(void) {
    sensor_radtemp_t = saul_reg_find_type(SAUL_SENSE_RADTEMP);
    if (sensor_radtemp_t == NULL) {
        DEBUG("[ERROR] Failed to init RADTEMP sensor\n");
        critical_error();
    } else {
        DEBUG("TEMP sensor OK\n");
    }

    sensor_hum_t     = saul_reg_find_type(SAUL_SENSE_HUM);
    if (sensor_hum_t == NULL) {
        DEBUG("[ERROR] Failed to init HUM sensor\n");
        critical_error();
    } else {
        DEBUG("HUM sensor OK\n");
    }

    sensor_temp_t    = saul_reg_find_type(SAUL_SENSE_TEMP);
    if (sensor_temp_t == NULL) {
		DEBUG("[ERROR] Failed to init TEMP sensor\n");
		critical_error();
	} else {
		DEBUG("TEMP sensor OK\n");
	}

    sensor_mag_t     = saul_reg_find_type(SAUL_SENSE_MAG);
    if (sensor_mag_t == NULL) {
		DEBUG("[ERROR] Failed to init MAGNETIC sensor\n");
		critical_error();
	} else {
		DEBUG("MAGNETIC sensor OK\n");
	}

    sensor_accel_t   = saul_reg_find_type(SAUL_SENSE_ACCEL);
    if (sensor_accel_t == NULL) {
		DEBUG("[ERROR] Failed to init ACCEL sensor\n");
		critical_error();
	} else {
		DEBUG("ACCEL sensor OK\n");
	}

    sensor_light_t   = saul_reg_find_type(SAUL_SENSE_LIGHT);
	if (sensor_light_t == NULL) {
		DEBUG("[ERROR] Failed to init LIGHT sensor\n");
		critical_error();
	} else {
		DEBUG("LIGHT sensor OK\n");
	}

    sensor_occup_t   = saul_reg_find_type(SAUL_SENSE_OCCUP);
	if (sensor_occup_t == NULL) {
		DEBUG("[ERROR] Failed to init OCCUP sensor\n");
		critical_error();
	} else {
		DEBUG("OCCUP sensor OK\n");
	}

    sensor_button_t  = saul_reg_find_type(SAUL_SENSE_BTN);
    if (sensor_button_t == NULL) {
        DEBUG("[ERROR] Failed to init BUTTON sensor\n");
        critical_error();
    } else {
        DEBUG("BUTTON sensor OK\n");
    }
}

int main(void) {
    uint16_t wakeup_count = 0;
    sensor_config();

    while (1) {
		xtimer_usleep(SAMPLE_INTERVAL);
        printf("I am Alive! %u\n", wakeup_count++);
    }

    return 0;
}
