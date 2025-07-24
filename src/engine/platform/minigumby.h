/**
 * Furnace Tracker - multi-system chiptune tracker
 * Copyright (C) 2021-2025 tildearrow and contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _MINMIGUMBY_H
#define _MINMIGUMBY_H

#include "../dispatch.h"
#include "../../fixedQueue.h"
#include "../waveSynth.h"
#include "sound/minnie.h"
#include <array>

class DivPlatformMiniGumby: public DivDispatch {
  struct Channel: public SharedChannel<signed char> {
    int noise;
    int noiseFreq;
    signed short wave;
    DivWaveSynth ws;
    Channel():
      SharedChannel<signed char>(56),
      wave(-1) {}
  };
  std::array<Channel,3> chan;
  std::array<DivDispatchOscBuffer*,3> oscBuf;
  std::array<bool,3> isMuted;
  struct QueuedWrite {
    unsigned short addr;
    unsigned char val;
    QueuedWrite(): addr(0), val(0) {}
    QueuedWrite(unsigned short a, unsigned char v): addr(a), val(v) {}
  };
  FixedQueue<QueuedWrite,256> writes;

  minnie_device* minnie;
  bool ramMode = false;
  bool noiseFreq = false;

  std::array<unsigned char,32> regPool;
  void updateROMWaves();
  void updateROMWave( size_t i );
  friend void putDispatchChip(void*,int);
  friend void putDispatchChan(void*,int,int);
  public:
    void acquire(short** buf, size_t len);
    int dispatch(DivCommand c);
    void* getChanState(int chan);
    DivMacroInt* getChanMacroInt(int ch);
    unsigned short getPan(int chan);
    DivDispatchOscBuffer* getOscBuffer(int chan);
    unsigned char* getRegisterPool();
    int getRegisterPoolSize();
    void reset();
    void forceIns();
    void tick(bool sysTick=true);
    void muteChannel(int ch, bool mute);
    int getOutputCount();
    bool keyOffAffectsArp(int ch);
    void setFlags(const DivConfig& flags);
    void notifyWaveChange(int wave);
    void notifyInsDeletion(void* ins);
    void poke(unsigned int addr, unsigned short val);
    void poke(std::vector<DivRegWrite>& wlist);
    const char** getRegisterSheet();
    int init(DivEngine* parent, int channels, int sugRate, const DivConfig& flags);
    void quit();
    ~DivPlatformMiniGumby();
};

#endif
