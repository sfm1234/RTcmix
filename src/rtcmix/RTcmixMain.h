#ifndef _RTCMIXMAIN_H_
#define _RTCMIXMAIN_H_

#include <RTcmix.h>

class RTcmixMain : public RTcmix {
public:
	RTcmixMain(int argc, char **argv);	// called from main.cpp
	void			run();	

protected:
	// Initialization methods.
	void			parseArguments(int argc, char **argv);
	static void		interrupt_handler(int);
	static void		signal_handler(int);
	static void		set_sig_handlers();

	static void *	sockit(void *);
	
private:
	static int 		xargc;	// local copy of arg count
	static char *	xargv[/*MAXARGS + 1*/];
	static int 		interrupt_handler_called;
	static int 		signal_handler_called;
	static int		noParse;
	#ifdef NETAUDIO
	static int		netplay;     // for remote sound network playing
	#endif
	/* for more than 1 socket, set by -s flag to CMIX as offset from MYPORT */
	static int		socknew;
};

#endif	// _RTCMIXMAIN_H_
