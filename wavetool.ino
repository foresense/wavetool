/*

WAVETOOL
by Robert Beenen

    Mod 1:    Pitch
    Mod 2:    Wavetable
	Button 1: Quantized slide
    Button 2: Interpolate
	Trigger:  Quantized slide

	Oscillator modeled after the Prophet VS for Ginko TOOL.
	I ripped the raw waveforms from my DSI Evolver (included Python scripts for this)
	added optional interpolate, a type of frequency lock, and a noise mode in the end.

	Thanks to Kassen Oud for the floating samplerate technique,
	and finding out about the buffered debouncing technique and XOR noise.

	Wavetable
	---------
	01 = Sinewave
	02 = Saw
	03 = Square
	..
	95 = Silence
	96 = Noise
	
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
bool trigger_state = false;
bool button1_state = false;
bool button2_state = false;

// internal
uint16_t mod1_glide;
uint16_t offset;
uint16_t offset_p;
uint8_t iratio;
uint8_t iratio_p;
uint8_t phase = 0;
bool interpolate = true;
bool noise = false;
bool qslide = false;
bool phase_clock = false;
bool phase_clock_act = false;
	
// output
uint16_t sample;
uint16_t timer1_preload;

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
		qslide = true;
	}
	else if (!trigger && trigger_state) {
		trigger_state = false;
		qslide = false;
	}
	if(button1 && !button1_state) {
		button1_state = true;
		qslide = !qslide;
	}
	else if (!button1 && button1_state) {
		button1_state = false;
	}
	if(button2 && !button2_state) {
		button2_state = true;
		interpolate = !interpolate;
	}
	else if (!button2 && button2_state) {
		button2_state = false;
	}
	if(!qslide) {
		mod1_glide = mod1;
		timer1_preload = pgm_read_word(&tuning_map[mod1]);
	}
	else {
		if (phase_clock && !phase_clock_act) {
			phase_clock_act = true;
			timer1_preload = pgm_read_word(&tuning_map[mod1_glide]);
		}
		else if (!phase_clock && phase_clock_act) {
			phase_clock_act = false;
		}
		if (mod1_glide > mod1) mod1_glide --;
		else if (mod1_glide < mod1) mod1_glide ++;
	}
								// extract wavetable offset and interpolation ratio out of the mod2 10-bit input
	offset = (mod2 >> 3) << 7; 	// lose first 3 bits, then multiply by 128 to get the offset in the wavetable
	iratio = mod2 & 0b111;		// first three bits are used to interpolate between wave -> 8 steps!

	if(offset == 12160 ) noise = true;
	else noise = false;

	// write LED status
	digitalWrite(LED1_PIN, qslide);
	digitalWrite(LED2_PIN, !interpolate);
}

// timer1 overflow interrupt
ISR(TIMER1_OVF_vect) {
	TCNT1 = timer1_preload;			// timer1 counter gets set immediately
	
	// only update the waveform at the end of a cycle, this sounds nicer
	if(phase & 0b10000000) {		// phase runs in 7 bits (128 steps).
		offset_p = offset;			// 8th bit signifies overflow
		iratio_p = iratio;
		phase &= 0b01111111;
		phase_clock = !phase_clock;	// tick tock
	}

	if(noise) {
		sample = rng() & 0x0FFF;	// 12 bits
	}
	else if(interpolate) {	// interpolate 2 waveforms with 8 steps (3 bits)
		sample = pgm_read_word(&wavetable[offset_p + phase]) * (uint8_t)(8 - iratio_p); 
		sample += pgm_read_word(&wavetable[offset_p + phase + 0x80]) * iratio_p;
		sample >>= 3;
	}
	else {
		sample = pgm_read_word(&wavetable[offset_p + phase]);
	}
	
	PORTB &= 0b11111011;					// open SPI for writing
	SPI.transfer((sample >> 8) | 0x30);		// sample ms nibble | DAC flag '0b0011'
	SPI.transfer(sample & 0x00FF);			// sample ls byte
	PORTB |= 0b00000100;					// close SPI
	
	phase ++;
}
