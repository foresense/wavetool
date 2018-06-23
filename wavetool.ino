/*

WAVETOOL
by Robert Beenen

    Mod 1:    Pitch
    Mod 2:    Wavetable
	Button 1: -
    Button 2: Interpolation
	Trigger:  -

	*/

//#define DEBUG

#include "SPI.h"
#include "tuning.h"
#include "evolver.h"

#define MOD1_PIN 		A1
#define MOD2_PIN 		A2
#define TRIGGER_PIN 	4
#define BUTTON1_PIN 	2
#define BUTTON2_PIN 	9
#define LED1_PIN 		3
#define LED2_PIN 		5
#define OUTPUT_PIN		10

// input
uint16_t mod1, mod2;
uint8_t trigger, button1, button2;

// internal
uint16_t offset, offset_p;
uint8_t ratio, ratio_p;
uint8_t phase = 0;
uint8_t step = 1;

// output
uint16_t timer1_preload;
uint16_t sample;

bool interpolation = true;

void setup() 
{
	cli();
	TCCR1A = 0b00000000;		// timer1 flags
	TCCR1B = 0b00000001;		// no prescaling
	TIMSK1 |= (1 << TOIE1);		// interrupt enable
	TCNT1 = 0;
	sei();

	pinMode(OUTPUT_PIN, OUTPUT);

	SPI.begin();
	SPI.setBitOrder(MSBFIRST);
}

void loop()
{
	mod1 = analogRead(MOD1_PIN);
	mod2 = (analogRead(MOD2_PIN) * 3) >> 2;		// 3/4 range: 96 waveforms

	ratio = mod2 & 0b111;
	offset = (mod2 >> 3) * 0x80;

	timer1_preload = pgm_read_word(&tuning_map[mod1]);
}

ISR(TIMER1_OVF_vect)
{
	
	/*
	
	the timer1 overflow interrupt (TIMER1_OVF_vect) is retriggered at the end of the
	trigger1 counter (TCNT1)
	
	*/

	TCNT1 = timer1_preload;

	// cycle length is 7 bits, the 8th bit signals overflow
	if(phase & 0b10000000)
	{
		offset_p = offset;
		ratio_p = ratio;
		phase &= 0b01111111;
	}

	if(interpolation)
	{
		// interpolate between 2 waveforms in 3 bits
		// the (uint8_t) cast saves cycles. avoid 16 bit math on an 8 bit cpu.
		sample = pgm_read_word(&wavetable[offset_p + phase]) * (uint8_t)(8 - ratio_p); 
		sample += pgm_read_word(&wavetable[offset_p + phase + 0x80]) * ratio_p;
		sample >>= 3;
	}
	else
	{
		sample = pgm_read_word(&wavetable[offset_p + phase]);
	}

										// 12 bit DAC
	PORTB &= 0b11111011;				// open SPI for writing
	SPI.transfer((sample >> 8) | 0x30);	// sample ms nibble + DAC flag '0b0011'
	SPI.transfer(sample & 0x00FF);		// sample ls byte
	PORTB |= 0b00000100;				// close SPI

	phase += step;
}
