include tables.info
include audio.info
include spacechain.info
include primes.info

load("MULTIWAVE")
load("HOLO")

sFudgeDiff = 0;

if (?seed) {
	srand($seed);
}

list gAmpCurves;
gEnv = makeline({0,0, 5,1, 1000,1, 1500,0.25, 1550,0});

handle rissetball(float dur, list ampcurves, float freq)
{
	inst = makeinstrument("MULTIWAVE", 0, dur, 1.0, wavet,
		freq * .56 + sFudgeDiff, ampcurves[0]*gEnv, irand(0, 359), 0,
		freq * .56 + 1 + sFudgeDiff, ampcurves[1]*gEnv * .67, irand(0, 359), 0,
		freq * .92 + sFudgeDiff, ampcurves[2]*gEnv, irand(0, 359), 0,
		freq * .92 + 1.7 + sFudgeDiff, ampcurves[3]*gEnv * 1.8, irand(0, 359), 0,
		freq * 1.19 + sFudgeDiff, ampcurves[4]*gEnv * 2.67, irand(0, 359), 0,
		freq * 1.7 + sFudgeDiff, ampcurves[5]*gEnv * 1.67, irand(0, 359), 0,
		freq * 2 + sFudgeDiff, ampcurves[6]*gEnv * 1.46, irand(0, 359), 0, 
		freq * 2.74 + sFudgeDiff, ampcurves[7]*gEnv * 1.33, irand(0, 359), 0,
		freq * 3 + sFudgeDiff, ampcurves[8]*gEnv * 1.33, irand(0, 359), 0,
		freq * 3.76 + sFudgeDiff, ampcurves[9]*gEnv, irand(0, 359), 0,
		freq * 4.07 + sFudgeDiff, ampcurves[10]*gEnv * 1.33, irand(0, 359), 0
	);
	return inst;
}

rvbtime = 1.3
mike_dist = 2.0;

configureRoom(47,65,54,10,rvbtime,mike_dist);

configureFrontInstrument("MULTIWAVE", 0);

setSpace(kMidground);

sFudgeDiff  = 0;

start = 0;
end = 0;

minamp = 0.01;
maxamp = 1.0;
minpoints = 3
maxpoints = 7

for (n = 0; n < 15; n += 1) {
	// a sine wave with extra harmonics for more complex bell sound
	wavet = maketable("wave", 5000, irand(0.8, 1.0), 0, irand(0.1, 0.4))

	dur = irand(1.45, 4.3);

	// each frequency component in the risset tone gets its own random gain curve
	gAmpCurves = {
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints)),
		randtable(minamp, maxamp, trand(minpoints, maxpoints))
	};
	// Play the metainstrument at frequency chosen from lowest portion of prime list
	inst = rissetball(dur, gAmpCurves, primeFrequencies[20+trand(0, len(primeFrequencies)/12)]);
	setFrontInstrument(inst);
	roomMoveArc(start, 0, dur, 100000, angle = pickrand(irand(-60, -30), irand(30, 60)), -angle/3);
	start += (dur - 0.1);
	end = start;
}


roomRun(end, 0.3);



