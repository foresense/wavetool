/*

PROPHECY
by Robert Beenen

    Mod 1:    Pitch
    Mod 2:    Wavetable
	Button 1: Frequency Lock
    Button 2: Interpolation
	Trigger:  Frequency Lock

	Oscillator modeled after the Prophet VS for Ginko TOOL.
	I ripped the raw waveforms from my DSI Evolver (included Python scripts for this)
	added optional interpolation, a type of frequency lock, and a noise mode in the end.

	Waveform 01 = Sinewave
	Waveform 02 = Saw
	Waveform 03 = Square
	...
	Waveform 95 = Silence
	Waveform 96 = Noise

	*/

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
uint16_t frequency_offset;
uint8_t ratio;
uint8_t ratio_p;
uint8_t phase = 0;

// state
bool trigger_state = false;
bool button1_state = false;
bool button2_state = false;
bool fixate = false;
bool interpolation = true;
bool noise = false;
	
// output
uint16_t timer1_preload;
uint16_t sample;

uint16_t seed = 1;
uint16_t rng(void) {
	if(!seed) seed++;
	seed ^= seed << 13;
	seed ^= seed >> 9;
	seed ^= seed << 7;
	return seed;
}

void setup() {
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

void loop() {
	mod1 = analogRead(MOD1_PIN);
	mod2 = (analogRead(MOD2_PIN) * 3) >> 2;
	trigger = (trigger << 1) | !digitalRead(TRIGGER_PIN);
	button1 = (button1 << 1) | !digitalRead(BUTTON1_PIN);
	button2 = (button2 << 1) | !digitalRead(BUTTON2_PIN);

	// set states
	if(trigger && !trigger_state) {
		trigger_state = true;
		frequency_offset = true;
	}
	else if (!trigger && trigger_state) {
		trigger_state = false;
		fixate = false;
	}
	if(button1 && !button1_state) {
		button1_state = true;
		fixate = !fixate;
	}
	else if (!button1 && button1_state) {
		button1_state = false;
	}
	if(button2 && !button2_state) {
		button2_state = true;
		interpolation = !interpolation;
	}
	else if (!button2 && button2_state) {
		button2_state = false;
	}

	// get preload time from tuning map
	if(fixate) {
		timer1_preload = pgm_read_word(&tuning_map[(mod1 & 0b1111100000) + frequency_offset]);
	}
	else {
		timer1_preload = pgm_read_word(&tuning_map[mod1]);
		frequency_offset = mod1 & 0b0000011111;
	}

	// set wavetable offset & interpolation ratio
	offset = (mod2 >> 3) << 7;
	ratio = mod2 & 0b111;

	if(offset == 12160 ) { 
		noise = true;
	}
	else {
		noise = false;
	}

	// write LED status
	digitalWrite(LED1_PIN, fixate);
	digitalWrite(LED2_PIN, !interpolation);
}

// timer1 overflow interrupt
ISR(TIMER1_OVF_vect) {
	
	TCNT1 = timer1_preload;	// timer1 counter gets reset immediately

	// phase runs in 7 bits (128 steps). 8th bit signals overflow.
	if(phase & 0b10000000) {
		offset_p = offset;
		ratio_p = ratio;
		phase &= 0b01111111;
	}

	if(noise) {
		sample = rng() & 0x0FFF;
	}
	// interpolate between 2 waveforms in 3 bits
	// the (uint8_t) cast saves cycles (avoid 16 bit math on an 8 bit cpu)
	else if(interpolation) {
		sample = pgm_read_word(&wavetable[offset_p + phase]) * (uint8_t)(8 - ratio_p); 
		sample += pgm_read_word(&wavetable[offset_p + phase + 0x80]) * ratio_p;
		sample >>= 3;
	}
	else {
		sample = pgm_read_word(&wavetable[offset_p + phase]);
	}
	
	PORTB &= 0b11111011;				// open SPI for writing
	SPI.transfer((sample >> 8) | 0x30);	// sample ms nibble + DAC flag '0b0011'
	SPI.transfer(sample & 0x00FF);		// sample ls byte
	PORTB |= 0b00000100;				// close SPI
	
	phase ++;
}
