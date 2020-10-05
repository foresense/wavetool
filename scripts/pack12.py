""" 
Pack evolver waveforms into 12 bits

Translate this:
0xf0, 0x7f, 0xd0, 0x7f,

To this:
0x7F, 0xF7, 0xFD

Or this:
0x60, 0x8c, 0x30, 0x8f

To this:
0x8C, 0x68, 0xF3 """

def word12(ms, ls):
	return (ms << 4) + (ls >> 4)

with open('evolver.raw', 'rb') as file:
	wavetable = file.read()

with open('evolver.12', 'wb') as file:
	for index, byte in enumerate(wavetable):
		if (index % 4) == 3:
			group = (a << 12) + word12(byte, low)
			prev = (a_prev << 16) + (low << 8) + byte
			print(f'{prev:#010x} -> {group:#08x}')
			file.write(bytes((group).to_bytes(3, byteorder='big')))
		elif (index % 2):
			a_prev = (low << 8) + byte
			a = word12(byte, low)
		else:
			low = byte
