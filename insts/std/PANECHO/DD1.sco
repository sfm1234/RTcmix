/* panecho: stereo panning delay instrument
*
*  p0 = output start time
*  p1 = input start time
*  p2 = duration
*  p3 = amplitude multiplier
*  p4 = channel 0 delay time
*  p5 = channel 1 delay time
*  p6 = regeneration multiplier (< 1!)
*  p7 = ring-down duration
*  p8 = input channel number <optional>
*  assumes function slot 1 is the amplitude envelope
*/

rtsetparams(44100, 2)
load("PANECHO")
rtinput("../../../snd/input.wav");
makegen(1, 24, 1000, 0,0, 0.5,1, 3.5,1, 7,0)
PANECHO(0, 0, 7, 1, .14, 0.069, .7, 3.5)

makegen(1, 24, 1000, 0,0, 0.1,1, 1.5,0.21, 3.5,1, 7,0)
PANECHO(4.9, 0, 7, 1, 3.14, 0.5, 0.35, 9.5)
