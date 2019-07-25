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


 /****************************************
 Macros
 ****************************************/
#define F_CPU 8000000UL		//Clock frequency is 8MHz.

#define ADC_value			ADCL

#define LED_brightness		0xFF					//The brightness of the LED from 0 to 255.
#define LED_on				TCCR0A &= ~(1<<COM0A1)	//The brightness of the LED is controlled with PWM so
#define LED_off				TCCR0A |= (1<<COM0A1)	//the LED should be turned on and off by
#define LED_toggle			TCCR0A ^= (1<<COM0A1)	//connecting/disconnecting OC0A to the port.

#define phone_on_pad		(PINB & (1<<PINB1))

 /****************************************
 Library import
 ****************************************/

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>


/****************************************
Initialization functions
****************************************/
void Set_pin_direction(void)
{
	DDRB |= (1<<DDB0);		//Set PB0 (pin 1 LED) to output.
	PUEB |= (1<<PUEB1);		//Enable pull-up on PB1. (Pin 2 connected to IR switch)
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

	OCR0AL = LED_brightness;		//Write the compare value to set LED brightness (0-255)

	TIMSK0 |= (1<<TOIE0);			//Enable overflow interrupt for tcnt0.
}


void (*state)(); //Function pointer

/****************************************
Function prototypes
****************************************/
uint16_t get_voltage();
void startup();
void carOn_phoneOff();
void carOn_phoneOn();
void carOff_phoneOn();
void carOff_phoneOff();
bool slow_blink(uint8_t num_of_blinks);
bool fast_blink(uint8_t num_of_blinks);


static volatile uint16_t cycle_timer_top = 15000;
volatile uint16_t cycle_timer = 0;
volatile bool adc_done = false;
static uint16_t voltage_limit_high = 14600;
static uint16_t voltage_limit_low = 14300;
uint16_t voltage = 0;

uint8_t ms_10_counter = 0;
uint16_t ms_500_counter = 0;
uint16_t ms_1000_counter = 0;

uint16_t old_cycle_timer = 0;


int main(void)
{
	Set_pin_direction();
	ADC_init();
	Timer_init();

	sei();
	TCCR0B |= (1<<CS00);			//Turn on clock with no prescaling.

	state = startup;

    while (1)
    {

		if (old_cycle_timer != cycle_timer)
		{
			ms_10_counter ++;
			ms_500_counter ++;
			ms_1000_counter ++;
		}
		old_cycle_timer = cycle_timer;

		if (ms_10_counter > 9)
		{
			ms_10_counter = 0;
		}
		if (ms_500_counter > 499)
		{
			ms_500_counter = 0;
		}
		if (ms_1000_counter > 999)
		{
			ms_1000_counter = 0;
		}
		
		if (ms_500_counter == 0)
		{
			adc_done = false;		//Ensures ADC conversion will happen aprox every 500ms.
		}


		if (!adc_done)
		{
			adc_done = true;
			SMCR |= (1<<SM0);		//Do an ADC conversion in ADC noise canceling mode.
									//CPU is halted while conversion is taking place
			voltage = get_voltage();							
		}
		state();
    }
}

ISR(TIM0_OVF_vect)
{
	cycle_timer ++;
	if (cycle_timer > cycle_timer_top)
	{
		cycle_timer = 0;
	}
}

/*****************************************
Functions
*****************************************/
uint16_t get_voltage()
{
	return ADC_value * 64; //Returns the voltage value in mV.
}

bool slow_blink(uint8_t num_of_blinks)
{
	static uint8_t blinks = 0;
	
	if (ms_1000_counter == 0)
	{
		LED_toggle;
		blinks ++;
	}

	if (blinks >= (num_of_blinks * 2))
	{
		return true;
	}
	else
	{
		return false;
	}
}
bool fast_blink(uint8_t num_of_blinks)
{
	static uint8_t blinks = 0;

	if (ms_10_counter == 0)
	{
		LED_toggle;
		blinks ++;
	}

	if (blinks >= (num_of_blinks * 2))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/*****************************************
State functions
*****************************************/
void startup()
{
	bool blink_finished = slow_blink(2);

	if (blink_finished && !phone_on_pad)
	{
		state = carOn_phoneOff;
	}
	else if (phone_on_pad)
	{
		state = carOn_phoneOn;
	}
	else if (voltage < voltage_limit_low)
	{
		state = carOff_phoneOff;
	}
}
void carOn_phoneOff()
{
	if (phone_on_pad)
	{
		state = carOn_phoneOn;
	}
	else if (voltage < voltage_limit_low)
	{
		state = carOff_phoneOff;
	}
}
void carOn_phoneOn()
{
	if (!phone_on_pad)
	{
		state = carOn_phoneOff;
	}
	else if (voltage < voltage_limit_low)
	{
		state = carOff_phoneOn;
	}
}
void carOff_phoneOn()
{
	if(!phone_on_pad)
	{
		state = carOff_phoneOff;
	}
	else if (voltage > voltage_limit_high)
	{
		state = carOn_phoneOn;
	}

	//TODO: Do blinking until power loss.
}
void carOff_phoneOff()
{
	if (phone_on_pad)
	{
		state = carOff_phoneOn;
	}
	else if (voltage > voltage_limit_high)
	{
		state = carOn_phoneOff;
	}
}