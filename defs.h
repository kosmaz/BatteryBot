/*
 * defs.h
 *
 *  Created on: Jul 4, 2016
 *      Author: kosmaz
 */

#ifndef DEFS_H_
#define DEFS_H_

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "lcd.h"

//#define TEST
#define F_CPU 12000000UL	//External clock frequency 12 MHz
#define HIGH 0x01	//8 bit value for 1
#define LOW 0x00	//8 bit value for 0
#define TRUE HIGH	//define our own true variable since C doesn't come with one by default
#define FALSE LOW	//define our own false variable since C doesn't come with one by default

#define BATTERY_LEVEL PA2	//Analog read pin for detecting voltage battery level
#define BUZZER_ON PORTA |= (1 << PA3)	//Turn ON buzzer
#define BUZZER_OFF PORTA &= ~(1 << PA3) 	//Turn OFF buzzer
#define EXTERNAL_POWER_AVAILABLE PINC & (1 << PC4)	//input pin used to check the availability of external power supply
#define BATTERY_CHARGE_ON PORTA |= (1 << PA1)	//Enable battery charging from external power supply
#define BATTERY_CHARGE_OFF PORTA &= ~(1 << PA1)	//Disable battery charging from external power supply
#define LOAD_SUPPLY_ON PORTA |= (1 << PA0)	//Enable power supply to a connected load
#define LOAD_SUPPLY_OFF PORTA &= ~(1 << PA0) //Disable power supply to a connected load
#define ENABLE_LED(a) PORTC |= (1 << a)	//enable or turn ON a LED bulb connected to pin a
#define DISABLE_LED(a) PORTC &= ~(1 << a)	//disable or turn OFF a LED bulb connected to pin a


/*************** MATRIX KEYPAD MAPPING START **********************/

//this enables a PIN by sending a HIGH signal to it
#define MATRIX_KEYPAD_OUTPUT_ENABLE(a) PORTB |= (1 << a)

//this disables a PIN by sending a LOW signal to it
#define MATRIX_KEYPAD_OUTPUT_DISABLE(b) PORTB &= ~(1 << b)

//this is used to scan a PIN register to indicate when a the pin has been set HIGH from user interaction
#define MATRIX_KEYPAD_INPUT_ENABLED(c) PINB & (1 << c)

/************** MATRIX KEYPAD MAPPING ENG *************************/

#define DEFAULT_SOC_VALUE 50	//default SOC value to be used by the module (unit = %)
#define BATTERY_MAX_VOLTAGE 12.0	//defines the maximum voltage of the battery in use (unit = V)


#define LCDInit() {\
	lcd_init();\
	lcd_on();\
	lcd_disable_blinking();\
	lcd_disable_cursor();\
	lcd_disable_autoscroll();\
}

#define LCDClear() lcd_clear()

#define LCDData(b) lcd_write(b)

#define LCDWriteStringXY(x, y, msg) {\
 lcd_set_cursor(x, y);\
 lcd_puts(msg);\
}

#define LCDWriteIntXY(x, y, val, fl) {\
 lcd_set_cursor(x, y);\
 LCDWriteInt(val,fl);\
}

void LCDWriteInt(int val,unsigned int field_length)
{
	/***************************************************************
	This function writes a integer type value to LCD module

	Arguments:
	1)int val	: Value to print

	2)unsigned int field_length :total length of field in which the value is printed
	must be between 1-5 if it is -1 the field length is no of digits in the val

	****************************************************************/

	char str[5] = {0, 0, 0, 0, 0};
	int i = 4, j = 0;
	while(val)
	{
		str[i] = val % 10;
		val = val / 10;
		i--;
	}
	if(field_length == -1)
		while(str[j] == 0) j++;
	else
		j = 5 - field_length;

	if(val < 0) LCDData('-');
	for(i = j; i < 5; i++)
	{
		LCDData(48 + str[i]);
	}
}


#endif /* DEFS_H_ */
