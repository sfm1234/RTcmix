// Copyright (C) 2005 John Gibson.  See ``LICENSE'' for the license to this
// software and for a DISCLAIMER OF ALL WARRANTIES.

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <ugens.h>   // for octpch and ampdb
#include <Ougens.h>  // for Ooscil
#include "grainstream.h"
#include "grainvoice.h"
#include <assert.h>

#define DEBUG 0
//#define NDEBUG       // disable asserts

#define ALL_VOICES_IN_USE  -1

#define COUNT_VOICES

const inline int _clamp(const int min, const int val, const int max)
{
   if (val < min)
      return min;
   if (val > max)
      return max;
   return val;
}


// NOTE: We don't own the table memory.
GrainStream::GrainStream(const float srate, double *inputTable, int tableLen,
   const int numInChans, const int numOutChans, const bool preserveGrainDur,
   const int seed)
   : _srate(srate), _inputtab(inputTable), _inputframes(tableLen / numInChans),
     _inchan(0), _winstart(-1), _winend(_inputframes), _wrap(true), _inhop(0),
     _outhop(0), _maxinjitter(0.0), _maxoutjitter(0.0), 
     _transp(0.0), _maxtranspjitter(0.0), _transptab(NULL), _transplen(0),
     _outframecount(0), _nextinstart(0), _nextoutstart(0), _travrate(1.0),
     _lastinskip(-DBL_MAX), _lastL(0.0f), _lastR(0.0f)
{
   for (int i = 0; i < MAX_NUM_VOICES; i++)
      _voices[i] = new GrainVoice(_srate, _inputtab, _inputframes, numInChans,
                                              numOutChans, preserveGrainDur);
   _inrand = new LinearRandom(0.0, 1.0, seed);
   _outrand = new LinearRandom(0.0, 1.0, seed * 2);
   _durrand = new LinearRandom(0.0, 1.0, seed * 3);
   _amprand = new LinearRandom(0.0, 1.0, seed * 4);
   _transprand = new LinearRandom(0.0, 1.0, seed * 5);
   _panrand = new LinearRandom(0.0, 1.0, seed * 6);
#ifdef COUNT_VOICES
   _maxvoice = -1;
#endif
}

GrainStream::~GrainStream()
{
   for (int i = 0; i < MAX_NUM_VOICES; i++)
      delete _voices[i];
   delete [] _transptab;
   delete _inrand;
   delete _outrand;
   delete _durrand;
   delete _amprand;
   delete _transprand;
   delete _panrand;
#ifdef COUNT_VOICES
   advise("GRANULATE", "Used %d voices", _maxvoice + 1);
#endif
}

// NOTE: We don't own the table memory.
// Call this before ever calling compute().
void GrainStream::setGrainEnvelopeTable(double *table, int length)
{
   for (int i = 0; i < MAX_NUM_VOICES; i++)
      _voices[i]->setGrainEnvelopeTable(table, length);
}

// Set inskip, overriding current position, which results from the traversal
// rate.  It's so important not to do this if the requested inskip hasn't
// changed that we do that checking here, rather than asking the caller to.

void GrainStream::setInskip(const double inskip)
{
   if (inskip != _lastinskip) {
      _nextinstart = int((inskip * _srate) + 0.5);
      _nextinstart = _clamp(_winstart, _nextinstart, _winend);
      _lastinskip = inskip;
   }
}

// Set input start and end point in frames.  If this is the first time we're
// called, force next call to compute() to start playing at start point.
// Otherwise, we'll come back to new start point at wraparound.

void GrainStream::setWindow(const double start, const double end)
{
   const bool firsttime = (_winstart == -1);

   _winstart = int((start * _srate) + 0.5);
   _winend = int((end * _srate) + 0.5);
   if (_winend < _winstart) {
      int tmp = _winstart;
      _winstart = _winend;
      _winend = tmp;
   }
   if (_winstart < 0)
      _winstart = 0;
   if (firsttime)
      _nextinstart = _winstart;

   // NOTE: _winend is the last frame we use (not one past the last frame)
   if (_winend >= _inputframes)
      _winend = _inputframes - 1;
}

// Set both the input traversal rate and the output grain hop.  Here are some
// possible values for <rate>, along with their interpretation.
//
//    0     no movement
//    1     move forward at normal rate (i.e., as fast as we hop through output)
//    2.5   move forward at a rate that is 2.5 times normal
//    -1    move backward at normal rate
//
// <hop> is the number of seconds to skip on the output before starting a
// new grain.  We add jitter to this amount in compute().

void GrainStream::setTraversalRateAndGrainHop(const double rate,
   const double hop)
{
   const double hopframes = hop * _srate;
   _outhop = int(hopframes + 0.5);
   _inhop = int((hopframes * rate) + 0.5);
   _travrate = rate;
//printf("inhop=%d, outhop=%d\n", _inhop, _outhop);
}

void GrainStream::setGrainTranspositionCollection(double *table, int length)
{
   delete [] _transptab;
   if (table) {
      _transptab = new double [length];
      for (int i = 0; i < length; i++)
         _transptab[i] = octpch(table[i]);
      _transplen = length;
      _transprand->setmin(0.0);
   }
   else {
      _transptab = NULL;
      _transplen = 0;
   }
}

// Given _transp and (possibly) _transptab, both in linear octaves, return
// grain transposition value in linear octaves.

const double GrainStream::getTransposition()
{
   double transp = _transp;
   const double transpjitter = (_maxtranspjitter == 0.0) ? 0.0
                                             : _transprand->value();
   if (_transptab) {
      // Constrain <transpjitter> to nearest member of transposition collection.
      double min = DBL_MAX;
      int closest = 0;
      for (int i = 0; i < _transplen; i++) {
         const double proximity = fabs(_transptab[i] - transpjitter);
         if (proximity < min) {
            min = proximity;
            closest = i;
         }
#if DEBUG > 1
         printf("transtab[%d]=%f, jitter=%f, proximity=%f, min=%f\n",
                           i, _transptab[i], transpjitter, proximity, min);
#endif
      }
      transp += _transptab[closest];
#if DEBUG > 0
      printf("transpcoll chosen: %f (linoct) at index %d\n",
                                             _transptab[closest], closest);
#endif
   }
   else {
      transp += transpjitter;
#if DEBUG > 0
      printf("transp (linoct): %f\n", transp);
#endif
   }

   return transp;
}

void GrainStream::playGrains(bool forwards)
{
   _lastL = 0.0f;
   _lastR = 0.0f;
   for (int i = 0; i < MAX_NUM_VOICES; i++) {
      if (_voices[i]->inUse()) {
         float sigL, sigR;
         _voices[i]->next(sigL, sigR, forwards);
         _lastL += sigL;
         _lastR += sigR;
      }
   }
}

// Return index of first freq grain voice, or -1 if none are free.
const int GrainStream::firstFreeVoice()
{
   for (int i = 0; i < MAX_NUM_VOICES; i++) {
      if (_voices[i]->inUse() == false)
         return i;
   }
   return ALL_VOICES_IN_USE;
}

// Compute one frame of samples across all active grains.  Activate a
// new grain if it's time.  Return false if caller should terminate
// prematurely, due to running out of input when not using wraparound mode;
// otherwise return true.

bool GrainStream::compute()
{
   bool keepgoing = true;

   if (_outframecount >= _nextoutstart) {
      // time to start another grain
      const int voice = firstFreeVoice();
      if (voice != ALL_VOICES_IN_USE) {
#ifdef COUNT_VOICES
         if (voice > _maxvoice)
            _maxvoice = voice;
#endif
         const double outdur = _durrand->value();
         const double amp = _amprand->value();
         const double pan = _panrand->value();
         const double transp = getTransposition();

         if (outdur >= 0.0)
            _voices[voice]->startGrain(_nextinstart, outdur, _inchan, amp,
                                                            transp, pan);
      }
      const int injitter = (_maxinjitter == 0.0) ? 0
                                          : int(_inrand->value() * _srate);
      _nextinstart += _inhop + injitter;

      // NB: injitter can be negative, and _inhop can be <= 0.
      if (_travrate < 0.0) {
         const int overshoot = _winstart - _nextinstart;
         if (overshoot >= 0) {
            if (_wrap)
               _nextinstart = _winend - overshoot;
            else
               keepgoing = false;
#if DEBUG > 0
            printf("wrap to ending... nextinstart=%d, overshoot=%d\n",
                                                   _nextinstart, overshoot);
#endif
         }
      }
      else {
         if (_nextinstart < _winstart)
            _nextinstart = _winstart;

         const int overshoot = _nextinstart - _winend;
         if (overshoot >= 0) {
            if (_wrap)
               _nextinstart = _winstart + overshoot;
            else
               keepgoing = false;
#if DEBUG > 0
            printf("wrap to beginning... nextinstart=%d, overshoot=%d\n",
                                                   _nextinstart, overshoot);
#endif
         }
      }

      const int outjitter = (_maxoutjitter == 0.0) ? 0
                                          : int(_outrand->value() * _srate);
      // NB: outjitter can be negative, so we must ensure that _nextoutstart
      // will not be less than the next _outframecount.
      _nextoutstart = _outframecount + _outhop + outjitter;
      if (_nextoutstart <= _outframecount)
         _nextoutstart = _outframecount + 1;
   }
#ifdef NOTYET  // not ready to try reverse read yet
   playGrains(_travrate >= 0.0);
#else
   playGrains(true);
#endif
   _outframecount++;

   return keepgoing;
}

