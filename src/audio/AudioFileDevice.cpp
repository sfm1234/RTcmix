// AudioFileDevice.cpp
//
// Allows applications to write audio to disk just as if they were writing
// to an audio device.  Performs sample fmt conversion between float and 
// other formats.  Constructor specifies audio file type, the file's sample 
// format, and options concerning format of data.

#include <math.h>       /* for fabs */
#include "AudioFileDevice.h"
#include <byte_routines.h>
#include <globals.h>	// MAXBUS, etc.
#include <assert.h>
#ifdef linux
#include <unistd.h>
#endif

struct AudioFileDevice::Impl {
	char						*path;
	int							frameSize;
	int							fileType;		// wave, aiff, etc.
	bool						checkPeaks;
	float						peaks[MAXBUS];
	long						peakLocs[MAXBUS];
};

AudioFileDevice::AudioFileDevice(const char *path,
								 int fileType,
								 int fileOptions)
	: _impl(new Impl)
{
	_impl->path = (char *)path;
	_impl->frameSize = 1024;
	_impl->fileType = fileType;
	_impl->checkPeaks = fileOptions & CheckPeaks;
	
}

AudioFileDevice::~AudioFileDevice()
{
//	printf("AudioFileDevice::~AudioFileDevice\n");
	close();
	delete _impl;
}

// We override open() in order to call setDeviceParams() BEFORE doOpen().

int AudioFileDevice::open(int mode, int fileSampFmt, int fileChans, double srate)
{
//	printf("AudioFileDevice::open\n");

	if ((mode & Record) != 0)
		return error("Record from file device not supported");
	
	for (int n = 0; n < MAXBUS; ++n) {
		_impl->peaks[n] = 0.0;
		_impl->peakLocs[n] = 0;
	}

	int status = 0;
	if (!isOpen()) {
		// Audio file formats are always interleaved.
		setDeviceParams((fileSampFmt & ~MUS_INTERLEAVE_MASK) | MUS_INTERLEAVED,
						fileChans,
						srate);
		if ((status = doOpen(mode)) == 0) {
			setState(Open);
			if ((status = doSetFormat(fileSampFmt, fileChans, srate)) != 0)
				close();
			else
				setState(Configured);
		}
		else {
			setState(Closed);
		}
	}
	return status;
}

// We override sendFrames() to allow peak scan before conversion of float.

int	AudioFileDevice::sendFrames(void *frameBuffer, int frames)
{
	if (IS_FLOAT_FORMAT(getDeviceFormat()) || _impl->checkPeaks) {
		int chans = getDeviceChannels();
		long bufStartSamp = frameCount();
		float *peaks = _impl->peaks;
		for (int c = 0; c < chans; ++c) {
			float *fp;
			int incr;
			if (isFrameInterleaved()) {
				fp = &((float *) frameBuffer)[c];
				incr = chans;
			}
			else {
				fp = ((float **) frameBuffer)[c];
				incr = 1;
			}
			for (int n = 0; n < frames; ++n, fp += incr) {
				double fabsamp = fabs((double) *fp);
				if (fabsamp > (double) peaks[c]) {
					peaks[c] = (float) fabsamp;
					_impl->peakLocs[c] = bufStartSamp + n;	// frame count
				}
			}
		}
	}
	return AudioDeviceImpl::sendFrames(frameBuffer, frames);
}

double AudioFileDevice::getPeak(int chan, long *pLocation)
{
	*pLocation = _impl->peakLocs[chan];
	return isDeviceFmtNormalized() ? _impl->peaks[chan] / 32768.0 : _impl->peaks[chan];
}

int AudioFileDevice::doOpen(int mode)
{
	assert(!(mode & Record));
	setMode(mode);
	
	int fd = sndlib_create((char *)_impl->path, _impl->fileType,
						   getDeviceFormat(), (int) getSamplingRate(), 
						   getDeviceChannels());
	setDevice(fd);
	closing(false);
	resetFrameCount();
	return (fd > 0) ? 0 : -1;
}

int AudioFileDevice::doClose()
{
	int status = 0;
	if (!closing()) {
		closing(true);
		if (_impl->checkPeaks) {
			// Normalize peaks if file was normalized.
			if (isDeviceFmtNormalized())
				for (int chan = 0; chan < getDeviceChannels(); ++chan)
					_impl->peaks[chan] /= 32768.0;
			(void) sndlib_put_header_comment(device(),
											 _impl->peaks,
											 _impl->peakLocs,
											 NULL);
		}
		// Last arg is samples not frames, for some archaic reason.
		status = sndlib_close(device(),
							  true,
							  _impl->fileType,
							  getDeviceFormat(),
							  getFrameCount() * getDeviceChannels());
		setDevice(-1);
	}
	return status;
}

int AudioFileDevice::doStart()
{
//	printf("AudioFileDevice::doStart: starting thread\n");
	return ThreadedAudioDevice::startThread();
}

int AudioFileDevice::doPause(bool paused)
{
	this->paused(paused);
	return 0;
}

int AudioFileDevice::doSetFormat(int sampfmt, int chans, double srate)
{
	return 0;
}

int AudioFileDevice::doSetQueueSize(int *pWriteSize, int *pCount)
{
	return 0;
}

int AudioFileDevice::doGetFrameCount() const
{
	return frameCount();
}

int AudioFileDevice::doGetFrames(void *frameBuffer, int frameCount)
{
	return error("Reading from file device not yet supported");
}

int	AudioFileDevice::doSendFrames(void *frameBuffer, int frames)
{
	int bytesToWrite = frames * getDeviceBytesPerFrame();
	int bytesWritten = ::write(device(), frameBuffer, bytesToWrite);
	if (bytesWritten < 0) {
		return error("Error writing to file.");
	}
	else if (bytesWritten < bytesToWrite) {
		return error("Incomplete write to file (disk full?)");
	}
	incrementFrameCount(frames);
	return frames;
}

void AudioFileDevice::run()
{
//	printf("AudioFileDevice::run: TOF\n");
	assert(!isPassive());	// Cannot call this method when passive!
	
	while (!stopping()) {
		while (paused()) {
#ifdef linux
			::usleep(1000);
#endif
		}
		if (runCallback() != true) {
//			printf("AudioFileDevice::run: callback returned false\n");
			break;
		}
	}
	// If we stopped due to callback being done, set the state so that the
	// call to close() does not attempt to call stop, which we cannot do in
	// this thread.  Then, check to see if we are being closed by the main
	// thread before calling close() here, to avoid a reentrant call.
	if (!stopping()) {
		setState(Configured);
		if (!closing()) {
//			printf("AudioFileDevice::run: calling close()\n");
			close();
		}
	}
//		printf("AudioFileDevice::run: calling stop callback\n");
	stopCallback();
//	printf("AudioFileDevice::run: thread exiting\n");
}

