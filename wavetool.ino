/*

WAVETOOL
by Robert Beenen

    Mod 1:    Pitch
    Mod 2:    Wavetable
	Button 1: Frequency Lock
    Button 2: Interpolation
	Trigger:  Frequency Lock

	*/

#define DEBUG

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
uint16_t mod1;
uint16_t mod2;
uint8_t trigger;
uint8_t button1;
uint8_t button2;

// internal
uint16_t offset;
uint16_t offset_p;
uint16_t fixate_offset;
uint8_t ratio;
uint8_t ratio_p;
uint8_t phase = 0;
uint8_t step = 1;
bool button1_state = false;
bool button2_state = false;
bool fixate = false;
bool interpolation = true;

#ifdef DEBUG
uint8_t debug_counter = 0;
#endif

// output
uint16_t timer1_preload;
uint16_t sample;


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

#ifdef DEBUG
	Serial.begin(9600);
	Serial.println("WAVETOOL DEBUG");
	Serial.println();
#endif
}

void loop()
{
	mod1 = analogRead(MOD1_PIN);
	if(fixate)
	{
		timer1_preload = pgm_read_word(&tuning_map[(mod1 & 0b1111100000) + fixate_offset]);
	}
	else
	{
		timer1_preload = pgm_read_word(&tuning_map[mod1]);
		fixate_offset = mod1 & 0b0000011111;
	}

	mod2 = (analogRead(MOD2_PIN) * 3) >> 2;		// reduce to 3/4 range
	ratio = mod2 & 0b111;
	offset = (mod2 >> 3) * 0x80;

	//trigger = (trigger << 1) | !digitalRead(TRIGGER_PIN);

	button1 = (button1 << 1) | !digitalRead(BUTTON1_PIN);
	if(button1 && !button1_state)	// rising edge
	{
		button1_state = true;
		fixate = !fixate;
	}
	else if (!button1)	// debounce technique
	{
		button1_state = false;
	}
	digitalWrite(LED1_PIN, fixate);
	
	button2 = (button2 << 1) | !digitalRead(BUTTON2_PIN);
	if(button2 && !button2_state)
	{
		button2_state = true;
		interpolation = !interpolation;
	}
	else if (!button2)
	{
		button2_state = false;
	}
	digitalWrite(LED2_PIN, interpolation);

#ifdef DEBUG
	debug_counter++;
	if(!debug_counter)
	{
		Serial.print("b1: ");
		Serial.print(button1);
		Serial.print(" | b2: ");
		Serial.print(button2);
		Serial.print(" | m1: ");
		Serial.print(mod1);
		Serial.print(" | m2: ");
		Serial.println(mod2);
	}
#endif
}

ISR(TIMER1_OVF_vect)	// timer1 overflow interrupt
{
	TCNT1 = timer1_preload;	// timer1 counter gets reset immediately

	if(phase & 0b10000000) // phase runs in 7 bits (128 steps). 8th bit signals overflow.
	{
		offset_p = offset;
		ratio_p = ratio;
		phase &= 0b01111111;
	}

	if(interpolation)
	{
		// interpolate between 2 waveforms in 3 bits
		// the (uint8_t) cast saves cycles (avoid 16 bit math on an 8 bit cpu)
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
