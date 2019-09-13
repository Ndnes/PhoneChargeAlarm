/*
 * PhoneChargeAlarm.c
 *
 * Created: 7/22/2019 11:32:47 AM
 * Author : Ndnes
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

#define LED_BRIGHTNESS			0xff					//The brightness of the LED from 0 to 255.
#define VOLTAGE_LIMIT_HIGH		14600	//mV
#define VOLTAGE_LIMIT_LOW		14300	//mV
#define CYCLE_TIMER_TOP			19//156					//Ensures top (155) is reached every 10ms if 
														//timer is configured for 9 bit fast pwm mode and F_CPU is 8MHz. For 1MHz f_cpu, top should be 20.

#define LED_on					TCCR0A |= (1<<COM0A1)	//The brightness of the LED is controlled with PWM so
#define LED_off					TCCR0A &= ~(1<<COM0A1)	//the LED should be turned on and off by
#define LED_toggle				TCCR0A ^= (1<<COM0A1)	//connecting/disconnecting OC0A to the LED pin.

#define counter_changed			(booleans & (1<<0))		//To save space all booleans are collected in 
#define set_counter_changed		(booleans |= (1<<0))	//one global uint8_t variable. These macros
#define clear_counter_changed	(booleans &= ~(1<<0))	//provides an interface to read and manipulate the
														//bits associated with each bool.
#define blink_done  			(booleans & (1<<1))
#define set_blink_done			(booleans |= (1<<1))
#define clear_blink_done		(booleans &= ~(1<<1))

#define fast_mode				(booleans & (1<<2))
#define set_fast_mode			(booleans |= (1<<2))
#define clear_fast_mode			(booleans &= ~(1<<2))
#define toggle_fast_mode		(booleans ^= (1<<2))

#define alarm_active			(booleans & (1<<3))		//Alarm will become active after activation timer has elapsed.
#define set_alarm_active		(booleans |= (1<<3))

#define activation_timer		(booleans >> 4)			//Use the spare 4 high bits of the 'booleans' global variable as 
#define incr_activation_timer   (booleans += 0b00010000)//an extra timer counter.

#define phone_off_pad		!(PINB & (1<<PINB1))

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
	ADCSRA |= (1<<ADEN);				//Enable the ADC.
	ADMUX |= (1<<MUX1);					//Sets ADC input to PB2.
	//ADCSRA |= (1<<ADIE);				//ADC conversion complete interrupt request enable.
	ADCSRA |= (1<<ADPS1) | (1<<ADPS0);	//Sets ADC prescaler to x8.

	DIDR0 |= (1<<ADC2D);				//Disable digital input buffer for ADC pin to reduce 
										//power consumption.
}
void Timer_init(void)
{
	TCCR0A |= (1<<WGM01);			//-
	TCCR0B |= (1<<WGM02);			//Sets up the timer for 9 bit fast PWM mode, top = 511.

	OCR0AL = LED_BRIGHTNESS;		//Write the compare value to set LED brightness (0-255)

	TIMSK0 |= (1<<TOIE0);			//Enable overflow interrupt for tcnt0.
}



/****************************************
Function prototypes
****************************************/
uint16_t get_voltage();
void (*state)();							//Function pointer
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
static volatile uint8_t cycle_timer = 0;	//Counter is incremented in timer TCNT0 overflow ISR

//TODO: REFACTOR Move variables to struct and pass to "state functions"
static volatile uint8_t ms_10_counter = 0; //counter is incremented every ~10ms
static volatile uint8_t booleans = 0b00000100; //Fast mode on by default.

uint16_t voltage = 0;		//Car voltage in mV
uint8_t blink_cnt = 0;



int main(void)
{
	Set_pin_direction();
	ADC_init();
	Timer_init();

	sei();						//Enable global interrupts.
	TCCR0B |= (1<<CS00);		//Turn on clock with no prescaling.

	state = startup;			//Initialize state

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
			if (ms_10_counter > 249)	//Counter needs to be reset when reaching 250 to prevent 
			{							//wrong timing on "overflow"
				//ms_10_counter = 0; Testing letting ms_10_counter overflow.
				set_alarm_active;
			}
			
			if ((ms_10_counter % 50) == 1)		//True every ~500ms Also true on first run.
			{
				ADCSRA |= (1<<ADSC);	//Do an ADC conversion in Single Conversion mode.
			}
			
			asm("nop");					//In testing there seems to been a timing issue with the following while loop,
			asm("nop");					//it does not seem to properly register the ADSC bit for a couple of cycles. Hence the no operation instructions.

			while (ADCSRA & (1<<ADSC)) {}

			voltage = ADC_value * 64;	//Voltage in mV. Conversion gets the actual car voltage and

			state();					//Execute the current state function.
			clear_counter_changed;		//Reset the counter_changed bool. It will be set in the
		}								//'TIM0_OVF_vect' ISR.
	}
}

ISR(TIM0_OVF_vect)			//TCNT0 overflow interrupt.
{
	cycle_timer ++;
	if (cycle_timer > CYCLE_TIMER_TOP)	//This should occur every 10ms
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
		if ((ms_10_counter % 64) == 1)
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
	if (ms_10_counter < 200)
	{
		set_fast_mode;
	}
	else
	{
		clear_fast_mode;
	}
	
	if (fast_mode)
	{
		if ((ms_10_counter % 4) == 0)
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
		LED_off;
		state = carOn_phoneOff;
	}
	else if (!phone_off_pad)
	{
		clear_blink_done;
		LED_off;
		state = carOn_phoneOn;
	}
	else if (!blink_done)
	{
		slow_blink(6);
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
	else if (alarm_active)	//'alarm_active' will be set high ~15 sec after power-on.
	{
		fast_blink();
	}
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