#include <Instrument.h>
#include <rtdefs.h>

class TRANS : public Instrument {
   int    incount, inframe, skip, inchan, get_frame;
   int    in_frames_left;
   double increment, counter;
   float  amp, pctleft;
   float  newsig, oldsig, oldersig;
   float  *amptable, tabs[2];
   float  *in;
public:
   TRANS();
   virtual ~TRANS();
   int init(float *, short);
   int run();
};

