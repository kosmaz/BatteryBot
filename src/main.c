/*
 * main.c
 *
 *  Created on: Jun 21, 2016
 *      Author: kosmaz
 */
#include <avr/io.h>
#include <util/delay.h>
#include <avr/interrupt.h>
#include "defs.h"

//NOTE: SOC stands for STATE OF CHARGE and is represented in % ranging from 0% - 100%

uint8_t gBuzzer_On = FALSE;
uint8_t gLoad_Supply_On = FALSE;	//indicates when power to the connected load is enabled
uint8_t gBattery_Charging = FALSE;	//indicates when the battery is being charged
uint8_t gCountdown_In_Progress = FALSE;	//indicates when count down has been started and is in progress

//global variables that will be modified by the ISR for TIMER1 OVERFLOW
volatile uint16_t gCountdown_Time = 0;	//used to hold the value for count down timing in minutes
volatile uint8_t gSeconds_Count = 59;	//used to hold the seconds count down
volatile uint16_t gMilli_Seconds = 0;	//used to hold the milliseconds count

/* used to hold the minimum battery voltage level that is set by the user to indicate when
 * supply to the connected load should be disconnected if the battery voltage level
 * in real time goes below this value
 */
uint16_t gSOC_Limit = DEFAULT_SOC_VALUE;

/* this pointer is to be used by float_to_string
 * routine to return values from memory when
 * called. This is as to solve the problem of
 * not being able to return an array by copying
 * but rather the pointer to the array can be
 * returned by copying. Returning pointer to
 * local variables isn't viable since the variable's
 * location in stack memory is cleaned after the
 * routine is through with its execution
 */
char* gString = NULL;

//ADC operations
static void ADC_init();
static uint16_t ADC_read(uint8_t);

//string operations
static uint16_t string_to_integer(char*);
static char* float_to_string(float, char);

//battery management operations
static void battery_manager();
static inline float battery_voltage_level();
static inline float soc_calculator();
static void led_display(float);

//settings operations
static void settings();
static void set_soc_limit();
static void set_countdown_time();

//time count down operations
static void setup_timer1();
static void init_countdown();
static void terminate_countdown();

//matrix keypad operations
static char scan_keypad_input(int16_t);

//main control
static void central_hub();


#ifndef TEST

int main()
{
	// Disable JTAG port
	MCUCSR = (1<<JTD);
	MCUCSR = (1<<JTD);

	//initialize ADC and LCD
	ADC_init();
	LCDInit();

	//initialize all required port pins to either input or output pin
	DDRA = 0b11111011;	//all pins except pin PA2 are output pins
	DDRB = 0b10001111;	//all pins except pins PB4, PB5, PB6 are output pins
	DDRC = 0b11101111;	//all pins except pin PC4 are output pins
	
	//disable all LED bulbs during initialization of the module
	DISABLE_LED(PC0);
	DISABLE_LED(PC1);
	DISABLE_LED(PC2);
	DISABLE_LED(PC3);

	//setup the TIMER1 counter which is to be used during count downs in the program
	setup_timer1();

	while(1)
		central_hub();

	return 0;
}

#endif

void ADC_init()
{
    // AREF = AVcc
    ADMUX = (1<<REFS0);

    // ADC Enable and prescaler of 128
    // 12000000/128 = 93750Hz
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    return;
}


// read ADC value
uint16_t ADC_read(uint8_t ch)
{
    /* select the corresponding channel 0~7
     * ANDing with '7' will always keep the value
     * of 'ch' between 0 and 7
     */
    ch &= 0b00000111;  // AND operation with 7
    ADMUX |= ch;

    /* start single conversion
     * write '1' to ADSC
     */
    ADCSRA |= (1 << ADSC);

    /* wait for conversion to complete
     * ADSC becomes '0' again
     * till then, run loop continuously
     */
    while(ADCSRA & (1 << ADSC));

    return (ADC);
}


uint16_t string_to_integer(char* string)
{
	/* This routine converts a NULL
	 * terminated string to a 16 bit
	 * unsigned integer
	 */
	uint16_t integer = 0;
	int string_length = strlen(string);
	for(int i = 0; i < string_length; ++i)
	{
		integer *= 10;
		integer += string[i] - '0';
	}
	return integer;
}


char* float_to_string(float value, char unit)
{
	/* This routine converts a float variable
	 * to a NULL terminated string. It also
	 * appends the unit character to the end
	 * of the string.
	 */
	int real_part = (int)value;
	uint8_t imag_part = (value - real_part) * 10;

	gString = calloc(7, 1);	//allocate a 7 byte memory location to gString
	strcpy(gString, "\0\0\0\0\0\0\0");

	uint8_t count = 0;

	int dummy = real_part;
	do
	{
		++count;
		dummy /= 10;
	}
	while(dummy);
	//reserve space for the . character as well as a single decimal character
	count += 2;

	if(unit != ' ')
		gString[count] = unit;
	gString[count + 1] = '\0';
	gString[--count] = (imag_part % 10) + '0';
	gString[--count] = '.';

	do
	{
		gString[--count] = (real_part % 10) + '0';
		real_part /= 10;
	}
	while(real_part);


	return gString;
}


void battery_manager()
{
	/* This routine manages every aspect of the battery
	 * component. It is able to determine when the battery
	 * is low thereby disconnecting the battery from the
	 * load. It is able to detect when the battery needs
	 * charging if there is an available external power
	 * supply. It is able to display the battery level status
	 * via LED bulbs and the LCD.
	 */

	led_display(soc_calculator());

	if((uint16_t)soc_calculator() < gSOC_Limit && !gBattery_Charging)
	{
		/* This block handles low battery
		 * situations by disconnecting the load
		 * from the battery. It displays the
		 * battery's SOC value to LCD. It handles
		 * special cases where the count down
		 * sequence is still running. It also handles
		 * triggering of the buzzer to indicate a
		 * low battery to the user
		 */
		if(!gBattery_Charging && !gBuzzer_On && (uint16_t)soc_calculator() < 45)
		{
			BUZZER_ON;
			gBuzzer_On = TRUE;
		}

		if(gLoad_Supply_On)
		{
			if(gCountdown_In_Progress)
				terminate_countdown();
			else
			{
				LOAD_SUPPLY_OFF;
				gLoad_Supply_On = FALSE;
			}
		}

		LCDClear();
		LCDWriteStringXY(2, 0, "BATTERY LOW");
		LCDWriteStringXY(4, 1, float_to_string(soc_calculator(), '%'));
		_delay_ms(300);
		//we need to free the memory used in holding the gString to avoid heavy memory leaks
		free(gString);
	}
	else if(!gCountdown_In_Progress && (uint16_t)soc_calculator() > gSOC_Limit && !gLoad_Supply_On)
	{
		/* This block handles situations where
		 * there is enough battery power. It
		 * connects the load to the battery and
		 * it also handles special cases where
		 * the count down sequence is still in
		 * phase
		 */
		if(gBuzzer_On)
		{
			BUZZER_OFF;
			gBuzzer_On = FALSE;
		}
		LOAD_SUPPLY_ON;
		gLoad_Supply_On = TRUE;
	}

	if(EXTERNAL_POWER_AVAILABLE)
	{
		/* This block handles battery charging
		 * when there is an external power supply.
		 */
		if(soc_calculator() >= 95.0 && gBattery_Charging)
		{
			BATTERY_CHARGE_OFF;
			gBattery_Charging = FALSE;
		}
		else if(soc_calculator() < 90.0 && !gBattery_Charging)
		{
			BATTERY_CHARGE_ON;
			gBattery_Charging = TRUE;
			if(gBuzzer_On)
			{
				BUZZER_OFF;
				gBuzzer_On = FALSE;
			}
		}

		LCDClear();
		LCDWriteStringXY(0, 0, "BATT CHARGING");
		LCDWriteStringXY(2, 1, "SOC = ");
		LCDWriteStringXY(8, 1, float_to_string(soc_calculator(), '%'));
		//we need to free the memory used in holding the gString to avoid heavy memory leaks
		free(gString);
		_delay_ms(200);
	}

	if(!gCountdown_In_Progress)
	{
		LCDClear();
		LCDWriteStringXY(0, 0, "SOC = ");
		LCDWriteStringXY(6, 0, float_to_string(soc_calculator(), '%'));
		//we need to free the memory used in holding the gString to avoid heavy memory leaks
		free(gString);
		LCDWriteStringXY(0, 1, "BATT = ");
		LCDWriteStringXY(7, 1, float_to_string(battery_voltage_level(), 'V'));
		_delay_ms(300);
		//we need to free the memory used in holding the gString to avoid heavy memory leaks
		free(gString);

		LCDClear();
		LCDWriteStringXY(0, 0, "SOC LIMIT = ");
		LCDWriteIntXY(12, 0, gSOC_Limit, 2);
		LCDWriteStringXY(14, 0, "%");
		_delay_ms(300);
	}

	return;
}


float battery_voltage_level()
{
	/* Read the ADC value from the BATTERY_LEVEL channel
	 * and convert it to a range of (0V - 12V) equivalent
	 * of the ADC reading (0V - 5V)
	 */
	return  (((float)ADC_read(BATTERY_LEVEL) * BATTERY_MAX_VOLTAGE) / 1023);
}


float soc_calculator()
{
	//convert the battery voltage reading to a percentage value
	return	((battery_voltage_level() / BATTERY_MAX_VOLTAGE) * 100.0);
}


void led_display(float level)
{
	/* This routine handles how many number
	 * of LED bulbs are turned ON or turned
	 * OFF based on the SOC value of the
	 * battery. It is used by batter_manager
	 * to properly display the battery level
	 * to the user
	 */
	if(level >= 85.0)
	{
		ENABLE_LED(PC0);
		DISABLE_LED(PC1);
		DISABLE_LED(PC2);
		DISABLE_LED(PC3);
	}
	else if(level < 85.0 && level >= 70.0)
	{
		DISABLE_LED(PC0);
		ENABLE_LED(PC1);
		DISABLE_LED(PC2);
		DISABLE_LED(PC3);
	}
	else if(level < 70.0 && level >= 55.0)
	{
		DISABLE_LED(PC0);
		DISABLE_LED(PC1);
		ENABLE_LED(PC2);
		DISABLE_LED(PC3);
	}
	else if(level < 55.0)
	{
		DISABLE_LED(PC0);
		DISABLE_LED(PC1);
		DISABLE_LED(PC2);
		ENABLE_LED(PC3);
	}
	return;
}


void settings()
{
	/* This routine handles collecting input
	 * settings from the user and calling the
	 * necessary sub-routine to handle user
	 * options
	 */
	LCDClear();
	LCDWriteStringXY(0, 0, "1. SET SOC LIMIT");
	LCDWriteStringXY(0, 1, "2. SET TIMER (m)");
	_delay_ms(300);

	LCDClear();
	LCDWriteStringXY(0, 0, "PRESS # > CANCEL");

	char input = '\0';
	input = scan_keypad_input(-1);
	//# character input to cancel user selection
	while(input != '1' && input != '2' && input != '#')
		input = scan_keypad_input(-1);
	switch(input)
	{
		case '1': {
			LCDWriteStringXY(7, 1, "1");	//echo user input on LCD
			_delay_ms(100);
			set_soc_limit();
			break;
		}
		case '2': {
			LCDWriteStringXY(7, 1, "2");	//echo user input on LCD
			_delay_ms(100);
			set_countdown_time();
			break;
		}
		case '#': break;
	}
	return;
}


void set_soc_limit()
{
	/* This routine handles setting the value
	 * of the SOC limit with the value provided
	 * by the user
	 */
	LCDClear();
	LCDWriteStringXY(0, 0, "SOC LIMIT VALUE:");

	char input[3] = "\0\0\0";
	int count = 0;
	char temp_input = '\0';
	do
	{
		temp_input = scan_keypad_input(-1);
		if((temp_input == '0' || temp_input == '1' || temp_input == '2' || temp_input == '3' || temp_input == '4')
				&& count == 0)
			continue;

		//let $ represent **
		if(temp_input == '*' || temp_input == '$')
			continue;

		//when the user wants to cancel the settings operation
		if(temp_input == '#')
			return;

		input[count++] = temp_input;

		//echo user input to LCD
		LCDWriteStringXY(0, 1, "                ");
		LCDWriteStringXY(6, 1, input);
		LCDWriteStringXY(6 + strlen(input), 1, "%        ");
	}
	while(count < 2);

	_delay_ms(100);
	input[count] = '\0';
	gSOC_Limit = string_to_integer(input);

	return;
}


void set_countdown_time()
{
	/* This routine handles setting the value
	 * of the count down time in minutes with
	 * the value provided by the user
	 */
	LCDClear();
	LCDWriteStringXY(0, 0, "PRESS # > CANCEL");
	LCDWriteStringXY(0, 1, "HOLD * TO START");

	char input[4] = "\0\0\0\0";
	int count = 0;
	char temp_input = '\0';
	do
	{
		temp_input = scan_keypad_input(-1);
		if(temp_input == '0' && count == 0)
			continue;
		if(temp_input == '*')
			continue;

		//when the user wants to cancel this setting operation
		if(temp_input == '#')
			return;

		/* when the user wants to start the count down process
		 * let $ represent **
		 */
		if(temp_input == '$' && count > 0)
			break;
		else if(temp_input == '$' && count == 0)
			continue;
		input[count++] = temp_input;

		//echo user input to LCD
		LCDWriteStringXY(0, 0, "                ");
		LCDWriteStringXY(3, 0, input);
		LCDWriteStringXY(3 + strlen(input), 0, " MIN(S)");
	}
	while(count < 3);

	_delay_ms(100);
	input[count] = '\0';
	gCountdown_Time = string_to_integer(input) - 1;
	init_countdown();	//start the count down process

	return;
}


void setup_timer1()
{
	/* use a prescaling of 64 (CLK = 12MHz / 64 = 187500Hz)
	 * Use CTC mode (clear timer on compare match)
	 */
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1<<CS10);
	OCR1A = 16;	//compare value (count from 0 - 16 and then reset to 0)

	sei();	//enable global interrupts

	return;
}


void init_countdown()
{
	/* This routine handles every that has to do with starting
	 * the TIMER1 counter. It initializes the variables
	 * used by the TIMER1 OVERFLOW ISR to emulate seconds count
	 * down in real time. It also writes the initial values of
	 * the count down sequence to LCD.
	 */
	gMilli_Seconds = 0;
	gSeconds_Count = 59;

	//get the number of hours from the user specified count down time which was give in minutes
	uint16_t hours = gCountdown_Time / 60;
	//get the corresponding number of minutes to tally with the number of hours calculated
	uint16_t mins = gCountdown_Time % 60;

	//write the initial values to LCD before the TIMER1 circuit is started
	LCDClear();
	LCDWriteIntXY(4, 0, hours, 2);
	LCDWriteStringXY(6, 0, ":");
	LCDWriteIntXY(7, 0, mins, 2);
	LCDWriteStringXY(9, 0, ":");
	LCDWriteIntXY(10, 0, gSeconds_Count, 2);

	LCDWriteStringXY(4, 1, "HH:");
	LCDWriteStringXY(7, 1, "MM:");
	LCDWriteStringXY(10, 1, "SS");
	gCountdown_In_Progress = TRUE;

	//Enable the Output Compare A interrupt
	TIMSK |= (1 << OCIE1A);

	return;
}


void terminate_countdown()
{
	LOAD_SUPPLY_OFF;
	gLoad_Supply_On = FALSE;

	//Disable the output compare A interrupt
	TIMSK &= ~(1 << OCIE1A);

	return;
}


ISR(TIMER1_COMPA_vect)
{
	/* TIMER1 Interrupt handler. Used to decrement
	 * the gCountdown_Time as well as display the
	 * progress on LCD. It counts down at a rate of
	 * 1 second in real-time. Some logic and calculation
	 * aids us to achieve this since the TIMER1 (16-bit)
	 * circuit can't give us the exact resolution needed
	 * to overflow every 1 second in real time.
	 */
	++gMilli_Seconds;

	if(gMilli_Seconds == 1000)
	{
		/* This block handles seconds count down when
		 * 1000 milliseconds has been reached
		 */
		--gSeconds_Count;
		LCDWriteIntXY(10, 0, gSeconds_Count, 2);
		gMilli_Seconds = 0;
	}

	if(gSeconds_Count == 0)
	{
		if(gCountdown_Time == 0)
		{
			/* This terminates the count down sequence and
			 * disconnects the load from the battery as it
			 * infinitely waits for a user interaction
			 * indicating a cancel operation.
			 */
			terminate_countdown();
			LCDWriteStringXY(0, 1, "PRESS # TO STOP");
			while(scan_keypad_input(-1) != '#');	//loop until user presses # to cancel the whole operation
			gCountdown_In_Progress = FALSE;
		}
		else
		{
			/* This block handles minute count down
			 * when  60 seconds has been reached
			 */
			--gCountdown_Time;
			gSeconds_Count = 59;

			//get the number of hours from the user specified count down time which was give in minutes
			uint16_t hours = gCountdown_Time / 60;
			//get the corresponding number of minutes to tally with the number of hours calculated
			uint16_t mins = gCountdown_Time % 60;

			//write the hours, minutes and seconds values to LCD
			LCDWriteIntXY(4, 0, hours, 2);
			LCDWriteIntXY(7, 0, mins, 2);
			LCDWriteIntXY(10, 0, gSeconds_Count, 2);

		}
	}
	return;
}


char scan_keypad_input(int16_t cycles)
{
	/* This routine handles getting user input from a matrix
	 * keypad connected to PORTB of the microcontroller.
	 */

	/* Need to handle situations where the number of cycles given
	 * is less than 0 which indicates an unlimited number of cycles
	 */
	int16_t count = (cycles < 0)? (-1 * cycles) : cycles;
	char input = '\0';

	while(count)
	{
		{
			MATRIX_KEYPAD_OUTPUT_ENABLE(PB0);
			_delay_ms(0.01);
			if(MATRIX_KEYPAD_INPUT_ENABLED(PB4))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB4));
				input = '1';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB0);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB5))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB5));
				input = '2';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB0);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB6))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB6));
				input = '3';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB0);
				break;
			}
			MATRIX_KEYPAD_OUTPUT_DISABLE(PB0);
		}

		{
			MATRIX_KEYPAD_OUTPUT_ENABLE(PB1);
			_delay_ms(0.01);
			if(MATRIX_KEYPAD_INPUT_ENABLED(PB4))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB4));
				input = '4';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB1);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB5))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB5));
				input = '5';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB1);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB6))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB6));
				input = '6';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB1);
				break;
			}
			MATRIX_KEYPAD_OUTPUT_DISABLE(PB1);
		}

		{
			MATRIX_KEYPAD_OUTPUT_ENABLE(PB2);
			_delay_ms(0.01);
			if(MATRIX_KEYPAD_INPUT_ENABLED(PB4))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB4));
				input = '7';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB2);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB5))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB5));
				input = '8';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB2);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB6))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB6));
				input = '9';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB2);
				break;
			}
			MATRIX_KEYPAD_OUTPUT_DISABLE(PB2);
		}

		{
			MATRIX_KEYPAD_OUTPUT_ENABLE(PB3);
			_delay_ms(0.01);
			if(MATRIX_KEYPAD_INPUT_ENABLED(PB4))
			{
				uint16_t internal_counts = 0;
				while((MATRIX_KEYPAD_INPUT_ENABLED(PB4)) &&  internal_counts < 600)
				{
					_delay_ms(0.1);
					++ internal_counts;
				}
				if(internal_counts > 599)
					input = '$';
				else
					input = '*';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB3);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB5))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB5));
				input = '0';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB3);
				break;
			}
			else if(MATRIX_KEYPAD_INPUT_ENABLED(PB6))
			{
				//keep on looping until user releases key
				while(MATRIX_KEYPAD_INPUT_ENABLED(PB6));
				input = '#';
				MATRIX_KEYPAD_OUTPUT_DISABLE(PB3);
				break;
			}
			MATRIX_KEYPAD_OUTPUT_DISABLE(PB3);
		}
		_delay_ms(0.1);

		//only decrement count if number of cycles is finite
		if(cycles > 0)
			--count;
	}
	return input;
}


void central_hub()
{
	/* Controls every aspect of the program
	 * from battery management to user input
	 * settings
	 */
	battery_manager();
	if(!gCountdown_In_Progress)
	{
		/* In other to avoid interrupting the count
		 * down process, we should check first to make
		 * sure that no count down is running before
		 * trying to ask for user input to the SOC limit
		 * and count down time settings
		 */
		LCDClear();
		LCDWriteStringXY(0, 0, "PRESS * > OPTION");

		//wait 5 cycles for user input before continuing execution
		char input = scan_keypad_input(5000);	//wait for user input for 5 seconds
		if(input == '*')
			settings();
	}
	return;
}
