#include <stdlib.h>
#include <iostream.h>
#include <Instrument.h>
#include "MYWAVETABLE.h"
#include <rt.h>

MYWAVETABLE::MYWAVETABLE() : Instrument()
{
}

MYWAVETABLE::~MYWAVETABLE()
{
}

int MYWAVETABLE::init(float p[], int n_args)
{
// p0 = start; p1 = dur; p2 = amplitude; p3 = frequency; p4 = stereo spread;

	nsamps = rtsetoutput(p[0], p[1], this);

	theOscil = new Ooscili(p[3], 2); // assumes makegen 2 for waveform

	theEnv = new Ooscili(1.0/p[1], 1); // assumes makegen 1 for amp env

	amp = p[2];
	spread = p[4];
	return(nsamps);
}

int MYWAVETABLE::run()
{
	int i;
	float out[2];
	
	for (i = 0; i < chunksamps; i++) {
		out[0] = theOscil->next() * theEnv->next() * amp;
		
		if (outputchans == 2) // split stereo files between channels
		{
			out[1] = (1.0 - spread) * out[0];
			out[0] *= spread;
		}
	  
		rtaddout(out);
		cursamp++;
		}
	return i;
}

double MYWAVETABLE::setfreq(double v)
{
	theOscil->setfreq((float)v);
	return v;
}

Instrument*
makeMYWAVETABLE()
{
	MYWAVETABLE *inst;
	inst = new MYWAVETABLE();
	inst->set_bus_config("MYWAVETABLE");
	return inst;
}

void
rtprofile()
{
	RT_INTRO("MYWAVETABLE",makeMYWAVETABLE);
}

