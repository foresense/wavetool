import math

LOW_C = 32.70
HIGH_C = 523.25
TIMER1_LENGTH = 0x1002C	# +45 offset, measured this. no idea why tbh.

def cycle_period(wavetable_length = 128, cpu = 16000000):
	return (1 / cpu) * wavetable_length

def div_factor(min_f, max_f, table_size = 1024):
	return table_size / math.log2((1 / min_f) * max_f)

def get_tuning(min_f, max_f, table_size = 1024):
	t = []
	cp = cycle_period(128, 16000000)
	df = div_factor(min_f, max_f, table_size)
	for n in range(table_size):
		p = 1 / (min_f * math.pow(2, n / df))
		t.append(TIMER1_LENGTH - round(p / cp))
	return t

get_tuning(LOW_C, HIGH_C)
