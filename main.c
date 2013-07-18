/* ---------------------------------------------------------------
 * Saturn Switchless Mod PIC Code (16F630/16F676)
 * Copyright (c) 2004 Sebastian Kienzl <seb@riot.org>
 * ---------------------------------------------------------------
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <htc.h>
#define _XTAL_FREQ 4000000

/*
EEPROM:

Byte
0		current country is saved here		
1		current 50/60-setting is saved here

these can be modified before burning:
2		colour for EU	(1=green, 2=red, 3=orange)
3		colour for USA
4		colour for JAP
*/

__CONFIG( MCLRDIS & BOREN & PWRTEN & WDTDIS & INTOSCIO & UNPROTECT );
__EEPROM_DATA( 0, 0, 1, 3, 2, 0, 0, 0 );
__EEPROM_DATA( '_', '_', 's', 'e', 'b', '!', '_', '_' );
__IDLOC(2702);

#define BUTTON	RA0
#define	VF		RA1

#define	RST		RA2
#define RST_T	TRISA2

char currCountry;
#define COUNTRYNUM 3

const char countries_JP[ COUNTRYNUM ] = {
	// JP12/JP10/JP6
	0b110,	// eu
	0b010,	// usa
	0b001	// japan
	//^^^_____ RC2/RC1/RC0 (JP12/JP10/JP6)
};

const char countries_VF[ COUNTRYNUM ] = {
	0,		// eu: 50hz
	1,		// usa: 60hz
	1		// japan: 60hz
};

char countries_COL[ COUNTRYNUM ] = {
	0b01,	// eu: green   (1)
	0b11,	// usa: orange (3)
	0b10	// japan: red  (2)
	//^^______ RC5/RC4 (red/green LED)
};

// approx. delay
void delay( int t )
{
	while( t-- ) {
		__delay_ms( 1 );
	}
}

void setCountry()
{
	PORTC = ( PORTC & 0b110000 ) |
			countries_JP[ currCountry ];
}

void setLeds()
{
	PORTC = ( PORTC & 0b000111 ) |
			( countries_COL[ currCountry ] << 4 );
}

void reset5060()
{
	VF = countries_VF[ currCountry ];
}

void darkenLeds( int msec )
{
	RC4 = 0;
	RC5 = 0;
	delay( msec );
	setLeds();
}

void display5060( char dunkel )
{
	if( !dunkel ) {
		darkenLeds( 200 );
	}

	if( VF == 0 ) {
		// 50Hz: blink once
		delay( 200 );
		darkenLeds( 200 );
	}
	else {
		int i;
		// 60Hz: blink rapidly a few times
		for( i = 0; i < 3; i++ ) {
			delay( 75 );
			darkenLeds( 75 );
		}
	}
}

void load()
{
	char c;
	int i;
	for( i = 0; i < COUNTRYNUM; i++ ) {
		c = eeprom_read( i + 2 );
		if( c >= 1 && c <= 3 ) {
			countries_COL[ i ] = c;
		}
	}

	currCountry = eeprom_read( 0 );
	if( currCountry >= COUNTRYNUM )
		currCountry = 0;
	setCountry();
	VF = eeprom_read( 1 );
	setLeds();
	display5060( 1 );
}

void save()
{
	eeprom_write( 0, currCountry );
	eeprom_write( 1, VF );
}

void reset()
{
	// put to LOW and output
	RST = 0;
	RST_T = 0;
	delay( 200 );
	// back to HIGH and hi-z
	RST_T = 1;
	RST = 1;
}



void main()
{
	// configure internal osc
	OSCCAL = _READ_OSCCAL_DATA();

	PCON = 0;           // we don't really care about POR/BOD
	CMCON = 0b111;      // disable comparator

	PORTA = 0;
	TRISA = 0b111101;
	// others are inputs, /RESET will only be configured to be an output on reset

	// configure wakeup for sleep
	RAIE = 1;
	IOCA0 = 1;          // Interrupt On Change on RA0 (Button)
//	GIE = 1;

	WPUA = 1;           // enable pullups (no floating)
	WPUA1 = 1;          // no pullups for /RESET
	RAPU = 0;           // enable individual pullups

	PORTC = 0;
	TRISC = 0b001000;   // JP/LED-outputs

	// settings laden
	load();

	while( 1 ) {
		// port change interrupt flag clear
		RAIF = 0;

		// wait until the button is pushed
		SLEEP();

		// debounce
		delay( 5 );

		// we only care about push
		if( BUTTON != 0 )
			continue;

		delay( 250 );

		if( BUTTON ) {
			// already released again -> normal reset
			reset();
		}
		else {
			darkenLeds( 1000 );

			if( BUTTON ) {
				// released only now: 50/60hz toggle
				VF ^= 1;
				save();
				display5060( 1 );
			}
			else {
				// change region?
				char change = 0;

				// display the current region for a while, so there's time to release the button
				delay( 1000 );

				// button still pushed? cycle regions while button is held
				while( !BUTTON ) {
					change = 1;
					if( ++currCountry >= COUNTRYNUM )
						currCountry = 0;
					setLeds();
					delay( 1000 );
				}

				if( change ) {
					// region change!
					setCountry();
					// set 50/60 accordingly and blink
					reset5060();
					save();
					display5060( 0 );
					reset();
				}
			}
		}
	}
}
