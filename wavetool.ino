/*	

WAVETOOL
Robert Beenen
foresense.org

	Mod 1:		Pitch
	Mod 2:		Wavetable
	Trigger:	
	Button 1:	
	Button 2:
	LED 1:		
	LED 2:

	*/

#include "SPI.h"
#include "kas-button.h"
#include "voltoctave.h"
#include "colundi.h"
#include "evolver.h"

#define MOD1_PIN 	A1
#define MOD2_PIN 	A2
#define TRIGGER_PIN 4
#define BUTTON1_PIN 2
#define BUTTON2_PIN 9
#define LED1_PIN 	3
#define LED2_PIN 	5
#define OUTPUT_PIN	10

// input
uint16_t mod1;
uint16_t mod2;
Button trigger;
Button button1;
Button button2;

// internal
bool colundi = false;
bool interpolation = true;
uint16_t offset, offset_p;
uint8_t ratio, ratio_p;
uint8_t phase = 0;
uint8_t step = 1;

// output
uint16_t timer1_preload;
uint16_t sample;

void setup() 
{
	// Serial.begin(9600);
	// Serial.println('WAVE DEBUG');

	cli();
	TCCR1A = 0b00000000;		// timer1 flags
	TCCR1B = 0b00000001;		// no prescaling
	TIMSK1 |= (1 << TOIE1);		// interrupt enable
	TCNT1 = 0;
	sei();

	pinMode(OUTPUT_PIN, OUTPUT);

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);

	pinMode(TRIGGER_PIN, INPUT_PULLUP);
	pinMode(BUTTON1_PIN, INPUT_PULLUP);
	pinMode(BUTTON2_PIN, INPUT_PULLUP);
	trigger.pin = TRIGGER_PIN;
	button1.pin = BUTTON1_PIN;
	button2.pin = BUTTON2_PIN;

	digitalWrite(LED1_PIN, colundi);
	digitalWrite(LED2_PIN, interpolation);
}

void loop()
{
	mod1 = analogRead(MOD1_PIN);
	mod2 = (analogRead(MOD2_PIN) * 3) >> 2;		// map 1024 to 768:
												// 96 waveforms, and 3 bits interpolation
	ratio = mod2 & 0b111;
	offset = (mod2 >> 3) * 0x7F;

	if(updateButton(&trigger))
	{
		if(trigger.state)
		{
		}
	}
	if(updateButton(&button1))
	{
		if(button1.state)
		{
			colundi = !colundi;
		}
		digitalWrite(LED1_PIN, colundi);
	}
	if(updateButton(&button2))
	{
		if(button2.state)
		{
			interpolation = !interpolation;
		}
		digitalWrite(LED2_PIN, interpolation);
	}

	if(colundi)
	{
		mod1 = (mod1 * 10) >> 8;	// map 0-1023 to 0-39
		switch(mod1)
		{
			case 35:
			case 36:
			case 37:
				step = 2;
				break;
			case 38:
				step = 8;
				break;
			case 39:
				step = 32;
				break;
			default:
				step = 1;
		}
		timer1_preload = pgm_read_word_near(colundiMap + mod1);
	}
	else
	{
		step = 1;
		timer1_preload = pgm_read_word_near(voltoctaveMap + mod1);
	}
}

ISR(TIMER1_OVF_vect)
{
	/*
	the timer1 interrupt (this function) is retriggered at the end of
	the timer1 counter. set this counter at the start of the interrupt
	so it will be used to set the update frequency, and the pitch.
	*/

	TCNT1 = timer1_preload;

	// cycle length is 7 bits, 8th bit is overflow
	if(phase & 0b10000000)
	{
		offset_p = offset;
		ratio_p = ratio;
		phase &= 0b01111111;
	}

	if(interpolation)
	{
		// interpolate between 2 waveforms in 3 bits
		sample = pgm_read_word(&wavetable[offset_p + phase]) * (8 - ratio_p);
		sample += pgm_read_word(&wavetable[offset_p + 0x7F + phase]) * ratio_p;
		sample >>= 3;
	}
	else
	{
		sample = pgm_read_word(&wavetable[offset_p + phase]);
	}

	PORTB &= 0b11111011;
	SPI.transfer(highByte(sample) | 0x30);	// 0x30 = tell the DAC to do something
	SPI.transfer(lowByte(sample));
	PORTB |= 0b00000100;

	phase += step;
}
