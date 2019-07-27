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

#define ADC_value				ADCL

#define LED_BRIGHTNESS			0x14					//The brightness of the LED from 0 to 255.
#define VOLTAGE_LIMIT_HIGH		14600
#define VOLTAGE_LIMIT_LOW		14300
#define CYCLE_TIMER_TOP			156

#define LED_on					TCCR0A |= (1<<COM0A1)	//The brightness of the LED is controlled with PWM so
#define LED_off					TCCR0A &= ~(1<<COM0A1)	//the LED should be turned on and off by
#define LED_toggle				TCCR0A ^= (1<<COM0A1)	//connecting/disconnecting OC0A to the port.

#define counter_changed			(booleans & (1<<0))
#define set_counter_changed		(booleans |= (1<<0))
#define clear_counter_changed	(booleans &= ~(1<<0))

#define blink_done  			(booleans & (1<<1))
#define set_blink_done			(booleans |= (1<<1))
#define clear_blink_done		(booleans &= ~(1<<1))

#define fast_mode				(booleans & (1<<2))
#define set_fast_mode			(booleans |= (1<<2))
#define clear_fast_mode			(booleans &= ~(1<<2))
#define toggle_fast_mode		(booleans ^= (1<<2))

#define phone_off_pad		(PINB & (1<<PINB1))

 /****************************************
 Library import
 ****************************************/

#include <avr/io.h>
#include <avr/interrupt.h>

//FOR DEBUGGING ONLY
//TODO: remove delay.h
//#include <util/delay.h>


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
	//TCCR0A |= (1<<COM0A1);			//Clear on compare match.***
	
	TCCR0A |= (1<<WGM01);			//-
	TCCR0B |= (1<<WGM02);			//Sets up the timer for 9 bit fast PWM mode, top = 511.

	OCR0AL = LED_BRIGHTNESS;		//Write the compare value to set LED brightness (0-255)

	TIMSK0 |= (1<<TOIE0);			//Enable overflow interrupt for tcnt0.
}



/****************************************
Function prototypes
****************************************/
uint16_t get_voltage();
void (*state)(); //Function pointer
void startup();
void carOn_phoneOff();
void carOn_phoneOn();
void carOff_phoneOn();
void carOff_phoneOff();
void slow_blink(uint8_t num_of_blinks);
void fast_blink();



/****************************************
Global Variables
****************************************/
static volatile uint8_t cycle_timer = 0;

//TODO: Move the following globals to a struct and pass to the "state functions"
static volatile uint8_t ms_10_counter = 0; //counter is incremented every ~10ms
static volatile uint8_t booleans = 0b00000100; //Fast mode on by default.

uint16_t voltage = 0;
uint8_t blink_cnt = 0;



int main(void)
{
	Set_pin_direction();
	ADC_init();
	Timer_init();

	sei();
	TCCR0B |= (1<<CS00);			//Turn on clock with no prescaling.

	state = startup;

	/*
	typedef struct data
	{
		uint16_t voltage;
		uint16_t ms_counter;
		uint8_t blink_cnt
		
	}data;
	*/


    while (1)
    {
		
		//Execute everything only once per ms_10_counter change. I.e every 10ms.
		if (counter_changed)
		{
			
			if (ms_10_counter > 249)
			{
				ms_10_counter = 0;
			}
			
			if ((ms_10_counter % 50) == 1)
			{
				SMCR |= (1<<SM0);	//Do an ADC conversion in ADC noise canceling mode every ~500ms
				voltage = ADC_value * 64; //Voltage in mV.
			}
			
			state();
			clear_counter_changed;
		}
	}
}

ISR(TIM0_OVF_vect)
{
	cycle_timer ++;
	if (cycle_timer > CYCLE_TIMER_TOP)
	{
		cycle_timer = 0;
		ms_10_counter ++;
		set_counter_changed;
	}
}

/*****************************************
Functions
*****************************************/

void slow_blink(uint8_t num_of_blinks)
{
	if (blink_cnt < (num_of_blinks * 2))
	{
		if ((ms_10_counter % 110) == 1)
		{
			LED_toggle;
			blink_cnt ++;
		}
	}
	else
	{
		set_blink_done;
		blink_cnt = 0;
	}
}
void fast_blink()
{
	if ((ms_10_counter % 100) == 0)
	{
		toggle_fast_mode;
	}
	
	if (fast_mode)
	{
		if ((ms_10_counter % 2) == 0)
		{
			LED_toggle;
		}
	}
	else
	{
		LED_off;
	}	
}

/*****************************************
State functions
*****************************************/
void startup()
{	
	if (blink_done && phone_off_pad)
	{
		clear_blink_done;
		state = carOn_phoneOff;
	}
	else if (!phone_off_pad)
	{
		clear_blink_done;
		state = carOn_phoneOn;
	}
	else if (voltage < VOLTAGE_LIMIT_LOW)
	{
		clear_blink_done;
		state = carOff_phoneOff;
	}
	else if (!blink_done)
	{
		slow_blink(3);
	}
}
void carOn_phoneOff()
{
	if (!phone_off_pad)
	{
		state = carOn_phoneOn;
	}
	else if (voltage < VOLTAGE_LIMIT_LOW)
	{
		state = carOff_phoneOff;
	}
}
void carOn_phoneOn()
{
	if (phone_off_pad)
	{
		state = carOn_phoneOff;
	}
	else if (voltage < VOLTAGE_LIMIT_LOW)
	{
		state = carOff_phoneOn;
	}
}
void carOff_phoneOn()
{
	if(phone_off_pad)
	{
		LED_off;
		state = carOff_phoneOff;
	}
	else if (voltage > VOLTAGE_LIMIT_HIGH)
	{
		LED_off;
		state = carOn_phoneOn;
	}

	fast_blink();
}
void carOff_phoneOff()
{
	if (!phone_off_pad)
	{
		state = carOff_phoneOn;
	}
	else if (voltage > VOLTAGE_LIMIT_HIGH)
	{
		state = carOn_phoneOff;
	}
}