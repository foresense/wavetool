import mido

midi_in = mido.open_input('MIDISPORT 8x8/s In 8 8')
midi_out = mido.open_output('MIDISPORT 8x8/s Out 8 9')

def get_shape(n):
	msg = mido.Message('sysex', data=[1,32,1,11,n])
	midi_out.send(msg)
	shape = midi_in.receive().data[5:]
	return unpack(shape)

def unpack(data):
	unpacked_data = []
	for index, byte in enumerate(data):
		packet_index = index % 8
		if (packet_index) == 0:
			ms_bits = byte
		else:
			if (ms_bits & (1 << (packet_index - 1))):
				byte = byte | 0x80
			unpacked_data.append(byte)
	return unpacked_data

# get all shapes and write to evolver.raw
with open('evolver.raw', 'wb') as wavetable:
	for n in range(96):
		shape = get_shape(n)
		wavetable.write(bytes(shape))

midi_in.close()
midi_out.close()
