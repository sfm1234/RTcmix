rtsetparams(44100, 2)
load("PVOC")

rtinput("../../snd/nucular.wav")

transposition_left = -0.01	// oct.pc
transposition_right = 0.01
timescale = 5.0
decimation = 20

start = 0
inskip = 0
dur = DUR() * timescale
amp = 2
inchan = 0
fftlen = 1024
window = 2 * fftlen
npoles = 0

bus_config("PVOC", "in 0", "out 0")
pitch_multiplier = cpspch(transposition_left) / cpspch(0.0)
PVOC(start, inskip, dur, amp, inchan, fftlen, window, decimation,
	decimation * timescale, pitch_multiplier, npoles)

bus_config("PVOC", "in 0", "out 1")
pitch_multiplier = cpspch(transposition_right) / cpspch(0.0)
PVOC(start, inskip, dur, amp, inchan, fftlen, window, decimation,
	decimation * timescale, pitch_multiplier, npoles)

