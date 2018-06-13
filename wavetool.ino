/*	

WAVETOOL
by Robert Beenen

	Mod 1:		Pitch
	Mod 2:		Wavetable
	Trigger:	
	Button 1:	
	Button 2:	
	LED 1:		
	LED 2:		

	*/

#include <avr/pgmspace.h>
#include <avr/interrupt.h>

#include "SPI.h"
#include "kas-button.h"
#include "voctave.h"
#include "colundi.h"
#include "evolver.h"

// hardware pins
#define MOD1_PIN 		A1
#define MOD2_PIN 		A2
#define TRIGGER_PIN 	4
#define BUTTON1_PIN 	2
#define BUTTON2_PIN 	9
#define LED1_PIN 		3
#define LED2_PIN 		5

// hardware variables
word mod1;
word mod2;
Button button1;
Button button2;
word output;

// internal variables
word pitch;
word pitch_phase;
word wave;
word wave_offset;
byte speed = 1;
byte speed_offset = 2;
byte phase = 0;
word sample;
word sample_i;
bool tuning = false;
bool full_cycle = true;

void setup()
{

	Serial.begin(9600);
	Serial.print("WAVETOOL DEBUG OUT - F_CPU: ");
	Serial.println(F_CPU);

	cli();
	TCCR1A = 0;
	TCCR1B = 1;
	TIMSK1 |= (1 << TOIE1);
	TCNT1 = 0;
	sei();

	pinMode(10, OUTPUT);
	
	SPI.begin();
	SPI.setBitOrder(MSBFIRST);

	//pinMode(TRIGGER_PIN, INPUT_PULLUP);
	pinMode(BUTTON1_PIN, INPUT_PULLUP);
	pinMode(BUTTON2_PIN, INPUT_PULLUP);
	button1.pin = BUTTON1_PIN;
	button2.pin = BUTTON2_PIN;
	
	digitalWrite(LED1_PIN, tuning);
	digitalWrite(LED2_PIN, full_cycle);

	TCNT1 = 65386;
}

//int linearMap(int input, int size_in, int size_out);

void loop()
{
	mod1 = analogRead(MOD1_PIN);
	mod2 = analogRead(MOD2_PIN);

	if(tuning)
	{
		mod1 = (mod1 * 10) >> 8;	// integer math: map 0-1023 -> 0-39
		switch(mod1) {
			case 35:
			case 36:
			case 37:
				speed = 4;
				break;
			case 38:
				speed = 16;
				break;
			case 39:
				speed = 64;
				break;
			default:
				speed = 2;	// full 128 bytes
				break;
		}
		pitch = pgm_read_word_near(colundi + mod1);
	}
	else
	{
		speed = 2;
		pitch = pgm_read_word_near(voctave + mod1);
	}
	
	mod2 = round((mod2 << 6) / 682);	// integer math: mapping 0-1023 -> 0-96
	wave = mod2 << 7;
	wave_i = (mod2 + 1) << 7;

	if(updateButton(&button1)) {
		if(button1.state) {
			tuning = !tuning;
		}
		digitalWrite(LED1_PIN, tuning);
	}
	if(updateButton(&button2)) {
		if(button2.state) {
			full_cycle = !full_cycle;
			phase = 0;
		}
		digitalWrite(LED2_PIN, full_cycle);
	}
}

ISR(TIMER1_OVF_vect)
{
	TCNT1 = pitch_phase;
	pitch_phase = pitch;

	// speed needs to change when it is aligned with phase at 0
	if(speed > speed_offset && !(phase % speed))
	{
		speed_offset = speed;
	}
	
	if(full_cycle && !phase)
	{
		wave_offset = wave;
		sample = pgm_read_word(&wavetable[wave_offset + (phase >> 1)]);
	}
	else if (!full_cycle)
	{
		speed_offset = speed;
		wave_offset = wave;
		sample = pgm_read_word
	}
	
	/*
	if(interpolate)
	{
		sample_i = sample & 0xfff;
		sample_i *= wave_interpolation ^ 0b111;
		sample_i = 

		sample += pgm_read_word(&wavetable[wave_offset + (phase >> 1) + 1]);
		sample >> 1;
	}
	*/

	PORTB &= 0b11111011;
	SPI.transfer(highByte(sample) | 0x30);
	SPI.transfer(lowByte(sample));
	PORTB |= 0b00000100;

	phase += speed_offset;
}
