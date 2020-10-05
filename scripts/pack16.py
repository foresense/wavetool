def word16(ms, ls):
	t = (ms << 4) + (ls >> 4) 
	t ^= 0x800
	t |= 0b0011000000000000
	return t

with open('evolver.raw', 'rb') as file:
	wavetable = file.read()

with open('evolver.16', 'wb') as file:
	for index, byte in enumerate(wavetable):
		if (index % 4) == 3:
			group = (a << 16) + word16(byte, low)
			prev = (a_prev << 16) + (low << 8) + byte
			print(f'{prev:#010x} -> {group:#010x}')
			file.write(bytes((group).to_bytes(4, byteorder='big')))
		elif (index % 2):
			a_prev = (low << 8) + byte
			a = word16(byte, low)
		else:
			low = byte
