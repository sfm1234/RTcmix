#include "strums.h"

class BEND : public Instrument {
	float freq0,freq1,diff;
	float tf0,tfN;
	float *glissf,tags[2];
	float spread;
	strumq *strumq1;
	int reset;

public:
	BEND();
	int init(float*, short);
	int run();
	};
