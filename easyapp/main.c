#include <stdio.h>
#include <rtt_stdio.h>
#include "xtimer.h"
#include <string.h>
#include "net/gnrc/udp.h"
#include "phydat.h"
#include "saul_reg.h"

#ifndef SAMPLE_INTERVAL
#define SAMPLE_INTERVAL ( 5000000UL)
#endif
#define SAMPLE_JITTER   ( 200000UL)

#define TYPE_FIELD 8

void send_udp(char *addr_str, uint16_t port, uint8_t *data, uint16_t datalen);

#define AES_SKIP_START_BYTES 4

typedef struct __attribute__((packed,aligned(4))) {
  uint16_t type;
  uint16_t serial;
  //From below is encrypted
  //We use a zero IV, so it is important that the first AES block
  //is completely unique, which is why we include uptime.
  //It is expected that hamilton nodes never reboot and that
  //uptime is strictly monotonic
  uint64_t uptime;
  uint16_t flags; //which of the fields below exist
  int16_t  acc_x;
  int16_t  acc_y;
  int16_t  acc_z;
  int16_t  mag_x;
  int16_t  mag_y;
  int16_t  mag_z;
  int16_t  ambtemp;
  int16_t  temp;
  int16_t  hum;
  int16_t  light_lux;
  uint16_t buttons;
  uint16_t occup;
  uint32_t reserved1;
  uint32_t reserved2;
  uint32_t reserved3;
} ham7c_t;

saul_reg_t *sensor_ambtemp_t = NULL;
saul_reg_t *sensor_temp_t    = NULL;
saul_reg_t *sensor_hum_t     = NULL;
saul_reg_t *sensor_mag_t     = NULL;
saul_reg_t *sensor_accel_t   = NULL;
saul_reg_t *sensor_light_t   = NULL;
saul_reg_t *sensor_occup_t   = NULL;
saul_reg_t *sensor_button_t  = NULL;

//Uptime is mandatory (for AES security)
#define FLAG_ACC    (1<<0)
#define FLAG_MAG    (1<<1)
#define FLAG_TMP    (1<<2)
#define FLAG_HDC    (1<<3)
#define FLAG_LUX     (1<<4)
#define FLAG_BUTTONS  (1<<5)
#define FLAG_OCCUP    (1<<6)

//All of the flags
#define PROVIDED_FLAGS (0x7F)

void critical_error(void) {
  printf("CRITICAL ERROR, REBOOT\n");
  NVIC_SystemReset();
  return;
}

void sensor_config(void) {
    sensor_ambtemp_t = saul_reg_find_type(SAUL_SENSE_AMBTEMP);   
    sensor_hum_t     = saul_reg_find_type(SAUL_SENSE_HUM);   
    sensor_temp_t    = saul_reg_find_type(SAUL_SENSE_TEMP);   
    sensor_mag_t     = saul_reg_find_type(SAUL_SENSE_MAG);   
    sensor_accel_t   = saul_reg_find_type(SAUL_SENSE_ACCEL);  
    sensor_light_t   = saul_reg_find_type(SAUL_SENSE_LIGHT); 
		sensor_occup_t   = saul_reg_find_type(SAUL_SENSE_OCCUP);
		sensor_button_t  = saul_reg_find_type(SAUL_SENSE_BTN);
}

/* ToDo: Sampling sequence arrangement or thread/interrupt based sensing may be better 
				 SAUL drivers need to detect malfunction 
*/
void sample(ham7c_t *m) {
    phydat_t output; /* Sensor output data (maximum 3-dimension)*/
		int dim;         /* Demension of sensor output */

		/* Occupancy */
		dim = saul_reg_read(sensor_occup_t, &output);
		m->occup = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_occup_t->name, saul_class_to_str(sensor_occup_t->driver->type));
		phydat_dump(&output, dim);

		/* Push button events */
		dim = saul_reg_read(sensor_button_t, &output); 
		m->buttons = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_button_t->name, 
						saul_class_to_str(sensor_button_t->driver->type));
    phydat_dump(&output, dim);

		/* Ambient temperature */
		dim = saul_reg_read(sensor_ambtemp_t, &output); /* 500ms */
		m->ambtemp = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_ambtemp_t->name, 
						saul_class_to_str(sensor_ambtemp_t->driver->type));
    phydat_dump(&output, dim);

		/* Temperature */
		dim = saul_reg_read(sensor_temp_t, &output); /* 15ms */
		m->temp = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_temp_t->name, saul_class_to_str(sensor_temp_t->driver->type));
		phydat_dump(&output, dim);
		
		/* Humidity */
		dim = saul_reg_read(sensor_hum_t, &output); /* 15ms */
		m->hum = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_hum_t->name, saul_class_to_str(sensor_hum_t->driver->type));
		phydat_dump(&output, dim);

		/* Illumination */
		dim = saul_reg_read(sensor_light_t, &output);
		m->light_lux = output.val[0];
    printf("\nDev: %s\tType: %s\n", sensor_light_t->name, saul_class_to_str(sensor_light_t->driver->type));
		phydat_dump(&output, dim);

		/* Magnetic field */
		dim = saul_reg_read(sensor_mag_t, &output);
		m->mag_x = output.val[0]; m->mag_y = output.val[1]; m->mag_z = output.val[2];
    printf("\nDev: %s\tType: %s\n", sensor_mag_t->name, saul_class_to_str(sensor_mag_t->driver->type));
		phydat_dump(&output, dim);

		/* Acceleration */
		dim = saul_reg_read(sensor_accel_t, &output);			
		m->acc_x = output.val[0]; m->acc_y = output.val[1]; m->acc_z = output.val[2];
    printf("\nDev: %s\tType: %s\n", sensor_accel_t->name, saul_class_to_str(sensor_accel_t->driver->type));
		phydat_dump(&output, dim);

		/* Time from start */
    m->uptime = xtimer_usec_from_ticks64(xtimer_now64());

		/* Others */
    m->serial = *fb_device_id;
    m->type   = TYPE_FIELD;
    m->flags  = PROVIDED_FLAGS;		
		
		puts("\n##########################\n");
}

uint32_t interval_with_jitter(void)
{
    int32_t t = SAMPLE_INTERVAL;
    t += rand() % SAMPLE_JITTER;
    t -= SAMPLE_JITTER / 2;
    return (uint32_t)t;
}

ham7c_t frontbuf;

uint8_t obuffer [sizeof(ham7c_t)];
uint8_t iv [16];

#include "crypto/ciphers.h"
#include "crypto/modes/cbc.h"
cipher_t aesc;

void crypto_init(void){
  //While this appears absurd, don't worry too much about it.
  //The first block is guaranteed to be unique so we don't really
  //need the IV
  for (int i = 0; i < 16; i++) {
    iv[i] = i;
  }
  //printf("us: %d\n", *fb_device_id);
  //printf("key: ");
  //for (int i = 0; i < 16; i++) {
  //  printf("%02x", fb_aes128_key[i]);
  //}
  //printf("\n");
  int rv = cipher_init(&aesc, CIPHER_AES_128, fb_aes128_key, 16);
  if (rv != CIPHER_INIT_SUCCESS) {
    printf("failed to init cipher\n");
    critical_error();
  }
}
void aes_populate(void) {
  cipher_encrypt_cbc(&aesc, iv, ((uint8_t*)&frontbuf) + AES_SKIP_START_BYTES, sizeof(ham7c_t)-AES_SKIP_START_BYTES, &obuffer[AES_SKIP_START_BYTES]);
  memcpy(obuffer, ((uint8_t*)&frontbuf), AES_SKIP_START_BYTES);
}


int main(void)
{
    //This value is good randomness and unique per mote
    srand(*((uint32_t*)fb_aes128_key));
    crypto_init();
    sensor_config();

  	printf("size %u\n", sizeof(ham7c_t));

    while (1) {
      //Sample
      LED_ON;
			sample(&frontbuf);
			LED_OFF;
   	  
      //aes_populate();
      //Send
      //send_udp("ff02::1",4747,obuffer,sizeof(obuffer));

      //Sleep
      xtimer_usleep(interval_with_jitter());
    }

    return 0;
}
