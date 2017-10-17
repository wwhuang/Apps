#include <stdio.h>
#include <rtt_stdio.h>
#include "xtimer.h"
#include <string.h>
#include "saul_reg.h"
// Sensor drivers. Should only need to touch these 2
#include "tmp006.h"
#include "tmp006_params.h"
#include "apds9007.h"
// RTC shit 
#include "periph/rtt.h"
#define ENABLE_DEBUG    (0)
#include "debug.h"
#include "periph/i2c.h"
#include "at86rf2xx.h"
#include "at86rf2xx_params.h"

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

/* driver asks for both of these even though params is inside tmp006_t */ 
tmp006_params_t tmpparams; 
tmp006_t m_tmp; 

apds9007_params_t apds_params;
apds9007_t m_apds; 

// Hm lets try and turn off the radio 
at86rf2xx_t m_rf; 

void rf_config(void) {
    at86rf2xx_setup(&m_rf, &at86rf2xx_params[0]);
    at86rf2xx_set_state(&m_rf, AT86RF2XX_STATE_SLEEP);
}

void critical_error(void) {
    DEBUG("CRITICAL ERROR, REBOOT\n");
    NVIC_SystemReset();
    return;
}

void sensor_config(void) {
    /* Configure the tmp006 */  
    // tmpparams taken from TMP006_PARAMS_BOARD in hamilton periph config file
    tmpparams.i2c = I2C_0;
    tmpparams.addr = 0x44;
    tmpparams.rate = TMP006_CONFIG_CR_AS2; 
    m_tmp.p = tmpparams;
    tmp006_init(&m_tmp, &tmpparams);
    tmp006_set_standby(&m_tmp);

    /* taken from the hamilton periph config file */
    apds_params.gpio = GPIO_PIN(PA,28); 
    apds_params.adc  = ADC_PIN_PA08;
    apds_params.res  = ADC_RES_16BIT;
    m_apds.p = apds_params;
    apds9007_set_idle(&m_apds);
}

void m_rtt_cb(void *arg) {
    LED_TOGGLE;
}

int main(void) {

    sensor_config(); // set up sensors 
    rtt_init(); // set up the rtt
    i2c_poweroff(0); // no need for this anymore.
    rf_config();

    while (1) {
        // Lets set the RTT to run on a 1s loop. 
        uint32_t c = rtt_get_counter();
        c += 70000; // alarm time should be 1 s in the future
        rtt_set_alarm(c, m_rtt_cb, NULL); // lets set a 1s alarm that does nothing else

        //LED_ON;

        // Set up PM clock masks so that we shut down stuff we don't need 
        uint32_t apbcmask = 0;
        PM->APBCMASK.reg = apbcmask; // don't need this clock

        uint32_t apbamask = 0;
        apbamask |= PM_APBAMASK_RTC;
        apbamask |= PM_APBAMASK_PM;
        //apbamask |= PM_APBAMASK_SYSCTRL; 
        PM->APBAMASK.reg = apbamask; 

        uint32_t apbbmask = 0; 
        //apbbmask |= PM_APBBMASK_NVMCTRL; 
        //apbbmask |= PM_APBBMASK_DSU; 
        apbbmask |= PM_APBBMASK_PORT; 
        PM->APBBMASK.reg = apbbmask;

        // Messing with AHBMASK does BAD things! Node semi-perminently enters a higher power state
        //uint32_t ahbmask = PM_AHBMASK_RESETVALUE; // not quite sure why this overlaps with apbbmask
        //PM->AHBMASK.reg = ahbmask;

        //xtimer_usleep(SAMPLE_INTERVAL); // this appears to be wired up to TIMER_1

        // I ripped out the code from cpu.h in cortexm_common. Also, I do not disable all interrupts.
        SCB->SCR |=  (SCB_SCR_SLEEPDEEP_Msk); // We go deep sleep!

        PM->SLEEP.reg = 2; // set idle level for not going to deep sleep
        //SCB->SCR &= ~(SCB_SCR_SLEEPDEEP_Msk); // we go not deep sleep (idle);
        /* ensure that all memory accesses have completed and trigger sleeping */
        __DSB();
        __WFI(); // This is the actual lets go to sleep function
        
        // RTC is on page 224 in the data sheet 
        // Generic Clock Controller is on page 90
        // Power manager on page 112
        // timers are page 568 
        // Long story short, we should use an RTC to sleep for a full second. 
		//xtimer_usleep(SAMPLE_INTERVAL); // this appears to be wired up to TIMER_1
    }

    return 0;
}
