#pragma once

#include <avr/io.h>

// Edit these
#define LCD_DDR  DDRD
#define LCD_PORT PORTD

#define LCD_RS 0
#define LCD_RW 1
#define LCD_EN 2
#define LCD_D0 3
#define LCD_D1 4
#define LCD_D2 5
#define LCD_D3 6

#define LCD_COL_COUNT 16
#define LCD_ROW_COUNT 2

// The rest should be left alone
#define LCD_CLEARDISPLAY   0x01
#define LCD_RETURNHOME     0x02
#define LCD_ENTRYMODESET   0x04
#define LCD_DISPLAYCONTROL 0x08
#define LCD_CURSORSHIFT    0x10
#define LCD_FUNCTIONSET    0x20
#define LCD_SETCGRAMADDR   0x40
#define LCD_SETDDRAMADDR   0x80

#define LCD_ENTRYRIGHT          0x00
#define LCD_ENTRYLEFT           0x02
#define LCD_ENTRYSHIFTINCREMENT 0x01
#define LCD_ENTRYSHIFTDECREMENT 0x00

#define LCD_DISPLAYON  0x04
#define LCD_DISPLAYOFF 0x00
#define LCD_CURSORON   0x02
#define LCD_CURSOROFF  0x00
#define LCD_BLINKON    0x01
#define LCD_BLINKOFF   0x00

#define LCD_DISPLAYMOVE 0x08
#define LCD_CURSORMOVE  0x00
#define LCD_MOVERIGHT   0x04
#define LCD_MOVELEFT    0x00

#define LCD_8BITMODE 0x10
#define LCD_4BITMODE 0x00
#define LCD_2LINE    0x08
#define LCD_1LINE    0x00
#define LCD_5x10DOTS 0x04
#define LCD_5x8DOTS  0x00

void lcd_init(void);

void lcd_command(uint8_t command);
void lcd_write(uint8_t value);

void lcd_on(void);
void lcd_off(void);

void lcd_clear(void);
void lcd_return_home(void);

void lcd_enable_blinking(void);
void lcd_disable_blinking(void);

void lcd_enable_cursor(void);
void lcd_disable_cursor(void);

void lcd_scroll_left(void);
void lcd_scroll_right(void);

void lcd_set_left_to_right(void);
void lcd_set_right_to_left(void);

void lcd_enable_autoscroll(void);
void lcd_disable_autoscroll(void);

void lcd_create_char(uint8_t location, uint8_t *charmap);

void lcd_set_cursor(uint8_t col, uint8_t row);

void lcd_puts(char *string);
void lcd_printf(char *format, ...);

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

