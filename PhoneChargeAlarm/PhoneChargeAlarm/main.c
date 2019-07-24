/*
 * PhoneChargeAlarm.c
 *
 * Created: 7/22/2019 11:32:47 AM
 * Author : Frakkmann Stasjonaer
 */ 


 /*
 ************************************************************
 ********************* Wiring Setup *************************
 ************************************************************
 *				   SOT-23
 *				-----------
 *		   PB0--|*		  |--PB3
 *				|         |
 *         GND--| ATtiny  |--VCC
 *				|	  10  |
 *		   PB1--|		  |--PB2
 *				-----------
 *
 *	GND - GND
 *	VCC - 5V
 *	PB0 - LED out
 *  PB1 - IR detector switch (Use pull-up) Pulls low when phone is off the pad.
 *	PB2 - ADC detect car battery voltage.
 *	PB3 - NC
 */

#define F_CPU 8000000UL		//Clock frequency is 8MHz.

#include <avr/io.h>
#include <avr/interrupt.h>

#define Start_ADC			ADCSRA |= (1<<ADSC)
#define Conv_in_progress	ADCSRA & (1<<ADIF)	 // TRUE: still converting FALSE: finished converting.
#define ADC_value			ADCL

#define LED_on				TCCR0A &= ~(1<<COM0A1)	//The brightness of the LED is controlled with PWM so
#define LED_off				TCCR0A |= (1<<COM0A1)	//the LED should be turned on and off by 
#define LED_toggle			TCCR0A ^= (1<<COM0A1)	//connecting/disconnecting OC0A to the port.

void Set_pin_direction(void)
{
	DDRB |= (1<<DDB0)		//Set PB0 (pin 1 LED) to output.
	PUEB |= (1<<PUEB1)		//Enable pull-up on PB1. (Pin 2 connected to IR switch)
}

void ADC_init(void)
{
	ADMUX |= (1<<MUX1);		//Sets ADC input to PB2.
	
	ADCSRA |= (1<<ADEN);				//Enable the ADC.
	ADCSRA |= (1<<ADIE);				//ADC conversion complete interrupt request enable.
	ADCSRA |= (1<<ADPS1) | (1<<ADPS0);	//Sets ADC prescaler to x8.

	DIDR0 |= (1<<ADC2D);				//Disable digital input buffer for ADC pin to reduce pwr consumption.
}

void Timer_init(void)
{
	TCCR0A |= (1<<COM0A1);			//Clear on compare match.***
	
	TCCR0A |= (1<<WGM00);			//-
	TCCR0B |= (1<<WGM02);			//Sets up the timer for 8 bit fast PWM mode.
	TCCR0B |= (1<<CS00);			//Turn on clock with no prescaling.

	OCR0AL = 0xFF;					//Write 255 to the low byte of OCR0A register. (2byte register)
}

int main(void)
{
	Set_pin_direction();
	ADC_init();
	Timer_init();

	sei();

	uint8_t LED_brightness = 0xFF; // Set duty cycle for LED light from 0 to 255.

    while (1) 
    {
    }
}

