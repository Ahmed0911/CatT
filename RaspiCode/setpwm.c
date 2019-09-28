#include <stdlib.h>
#include <stdio.h>
#include <wiringPi.h> /* include wiringPi library */
#include <softPwm.h>  /* include header file for software PWM */

int main(int argc, char *argv[])
{
        int PWM_pin = 18;               /* GPIO1 as per WiringPi,GPIO18 as per BCM */
        int intensity;
        wiringPiSetupSys();             /* initialize wiringPi setup, uses BCM, GPIO18 */                                                                                                                                
        pinMode(PWM_pin,OUTPUT);        /* set GPIO as output */
        softPwmCreate(PWM_pin,0,200);   /* set PWM channel, RANGE: 200 * 0.1us = 20ms, dafult: 0 - OFF) */

        delay(100);

        int position = 15; // middle
        if( argc > 1 )
        {
                // extact position
                position = atoi(argv[1]);
        }

        // set servo position
        softPwmWrite(PWM_pin, position);

        // wait till position is reached
        delay(1000);

        // kill
        // NOTE: does not work with  G-SUN, must cycle power
        softPwmWrite(PWM_pin, 0);
        delay(100);
}