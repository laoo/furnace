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

#include "minigumby.h"
#include "../engine.h"
#include <math.h>

 //#define rWrite(a,v) pendingWrites[a]=v;
#define rWrite(a,v) if (!skipRegisterWrites) {writes.push(QueuedWrite(a,v)); if (dumpWrites) {addWrite(a,v);} }

#define SAMPLING_FREQUENCY (28000)
#define CHIP_FREQBASE (65536 / 16)


static constexpr char hex_digit_to_char( int digit )
{
  return digit < 10 ? static_cast<char>( '0' + digit ) : static_cast<char>( 'A' + digit - 10 );
}

template<int value>
struct hex_to_string
{
  static constexpr char buf[] = {
    hex_digit_to_char( ( value >> 4 & 0x0f ) ),
    hex_digit_to_char( value & 0x0f ),
    '\0'
  };
};

const char** DivPlatformMiniGumby::getRegisterSheet()
{
  static char const* regCheatSheetMinnie[] = {
    "Freq1L",   hex_to_string<minnie_device::FREQ1L>::buf,
    "Freq1H",   hex_to_string<minnie_device::FREQ1H>::buf,
    "Vol1",     hex_to_string<minnie_device::VOL1>::buf,
    "Timbre1",  hex_to_string<minnie_device::TIMBRE1>::buf,
    "Index1L",  hex_to_string<minnie_device::INDEX1L>::buf,
    "Index1H",  hex_to_string<minnie_device::INDEX1H>::buf,
    "Freq2L",   hex_to_string<minnie_device::FREQ2L>::buf,
    "Freq2H",   hex_to_string<minnie_device::FREQ2H>::buf,
    "Vol2",     hex_to_string<minnie_device::VOL2>::buf,
    "Timbre2",  hex_to_string<minnie_device::TIMBRE2>::buf,
    "Index2L",  hex_to_string<minnie_device::INDEX2L>::buf,
    "Index2H",  hex_to_string<minnie_device::INDEX2H>::buf,
    "Freq3L",   hex_to_string<minnie_device::FREQ3L>::buf,
    "Freq3H",   hex_to_string<minnie_device::FREQ3H>::buf,
    "Vol3",     hex_to_string<minnie_device::VOL3>::buf,
    "Timbre3",  hex_to_string<minnie_device::TIMBRE3>::buf,
    "Index3L",  hex_to_string<minnie_device::INDEX3L>::buf,
    "Index3H",  hex_to_string<minnie_device::INDEX3H>::buf,
    NULL
  };

  return regCheatSheetMinnie;
}

void DivPlatformMiniGumby::acquire( short** buf, size_t len )
{
  while ( !writes.empty() )
  {
    QueuedWrite w = writes.front();
    minnie->poke( w.addr, w.val );
    regPool[w.addr & 0x1f] = w.val;
    writes.pop();
  }

  for ( size_t i = 0; i < oscBuf.size(); i++ )
  {
    oscBuf[i]->begin( len );
  }

  for ( size_t h = 0; h < len; h++ )
  {
    short chanBuf[3];

    buf[0][h] = minnie->sample_audio( chanBuf );
    for ( int i = 0; i < oscBuf.size(); i++ )
    {
      oscBuf[i]->putSample( h, chanBuf[i] );
    }
  }

  for ( int i = 0; i < oscBuf.size(); i++ )
  {
    oscBuf[i]->end( len );
  }
}

void DivPlatformMiniGumby::tick( bool sysTick )
{
  for ( int i = 0; i < chan.size(); i++ )
  {
    chan[i].std.next();
    if ( chan[i].std.vol.had )
    {
      chan[i].outVol = VOL_SCALE_LINEAR( chan[i].vol, chan[i].std.vol.val, 56 );
    }
    if ( chan[i].std.duty.had )
    {
      chan[i].noise = chan[i].std.duty.val;
      chan[i].freqChanged = true;
    }
    if ( NEW_ARP_STRAT )
    {
      chan[i].handleArp();
    }
    else if ( chan[i].std.arp.had )
    {
      if ( !chan[i].inPorta )
      {
        chan[i].baseFreq = NOTE_FREQUENCY( parent->calcArp( chan[i].note, chan[i].std.arp.val ) );
      }
      chan[i].freqChanged = true;
    }
    if ( chan[i].std.wave.had )
    {
      if ( chan[i].wave != chan[i].std.wave.val || chan[i].ws.activeChanged() )
      {
        chan[i].wave = chan[i].std.wave.val;
        chan[i].ws.changeWave1( chan[i].wave );
        if ( !chan[i].keyOff ) chan[i].keyOn = true;
      }
    }
    if ( chan[i].std.pitch.had )
    {
      if ( chan[i].std.pitch.mode )
      {
        chan[i].pitch2 += chan[i].std.pitch.val;
        CLAMP_VAR( chan[i].pitch2, 0, 65535 );
      }
      else
      {
        chan[i].pitch2 = chan[i].std.pitch.val;
      }
      chan[i].freqChanged = true;
    }
    if ( chan[i].active )
    {
      if ( chan[i].ws.tick() || ( chan[i].std.phaseReset.had && chan[i].std.phaseReset.val == 1 ) )
      {
        updateROMWave( i );
      }
    }
    if ( chan[i].freqChanged || chan[i].keyOn || chan[i].keyOff )
    {
      //DivInstrument* ins=parent->getIns(chan[i].ins,DIV_INS_PCE);
      chan[i].freq = parent->calcFreq( chan[i].baseFreq, chan[i].pitch, chan[i].fixedArp ? chan[i].baseNoteOverride : chan[i].arpOff, chan[i].fixedArp, false, 2, chan[i].pitch2, chipClock, CHIP_FREQBASE );
      if ( chan[i].freq < 0 ) chan[i].freq = 0;
      if ( chan[i].freq > 65535 ) chan[i].freq = 65535;
      if ( chan[i].keyOn )
      {
      }
      if ( chan[i].keyOff )
      {
      }
      if ( chan[i].keyOn ) chan[i].keyOn = false;
      if ( chan[i].keyOff ) chan[i].keyOff = false;
      chan[i].freqChanged = false;
    }
  }

  // update state
  if ( chan[0].active && !isMuted[0] )
  {
    uint8_t v = chan[0].outVol & 0x0f | ( 0x40 - chan[0].outVol - 1 ) & 0xf0;
    uint8_t n = chan[0].noise ? 0x07 + chan[0].noise : 0;
    rWrite( minnie_device::VOL1, v );
    rWrite( minnie_device::TIMBRE1, ( chan[0].wave & 7 ) | ( n << 4 ) );
  }
  else
  {
    rWrite( minnie_device::TIMBRE1, 4 );
  }
  if ( chan[1].active && !isMuted[1] )
  {
    uint8_t v = chan[1].outVol & 0x0f | ( 0x40 - chan[1].outVol - 1 ) & 0xf0;
    uint8_t n = chan[1].noise ? 0x07 + chan[0].noise : 0;
    rWrite( minnie_device::VOL2, v );
    rWrite( minnie_device::TIMBRE2, ( chan[1].wave & 7 ) | ( n << 4 ) );
  }
  else
  {
    rWrite( minnie_device::TIMBRE2, 4 );
  }
  if ( chan[2].active && !isMuted[2] )
  {
    uint8_t v = chan[2].outVol & 0x0f | ( 0x40 - chan[2].outVol - 1 ) & 0xf0;
    uint8_t n = chan[2].noise ? 0x07 + chan[0].noise : 0;
    rWrite( minnie_device::VOL3, v );
    rWrite( minnie_device::TIMBRE3, ( chan[2].wave & 7 ) | ( n << 4 ) );
  }
  else
  {
    rWrite( minnie_device::TIMBRE3, 4 );
  }

  rWrite( minnie_device::FREQ1L, ( chan[0].freq ) & 0xff );
  rWrite( minnie_device::FREQ1H, ( chan[0].freq >> 8 ) & 0xff );
  rWrite( minnie_device::FREQ2L, ( chan[1].freq ) & 0xff );
  rWrite( minnie_device::FREQ2H, ( chan[1].freq >> 8 ) & 0xff );
  rWrite( minnie_device::FREQ3L, ( chan[2].freq ) & 0xff );
  rWrite( minnie_device::FREQ3H, ( chan[2].freq >> 8 ) & 0xff );
}

int DivPlatformMiniGumby::dispatch( DivCommand c )
{
  switch ( c.cmd )
  {
  case DIV_CMD_NOTE_ON:
  {
    DivInstrument* ins = parent->getIns( chan[c.chan].ins, DIV_INS_PCE );
    if ( c.value != DIV_NOTE_NULL )
    {
      chan[c.chan].baseFreq = NOTE_FREQUENCY( c.value );
      chan[c.chan].freqChanged = true;
      chan[c.chan].note = c.value;
    }
    chan[c.chan].active = true;
    chan[c.chan].keyOn = true;
    chan[c.chan].macroInit( ins );
    if ( !parent->song.brokenOutVol && !chan[c.chan].std.vol.will )
    {
      chan[c.chan].outVol = chan[c.chan].vol;
    }
    if ( chan[c.chan].wave < 0 )
    {
      chan[c.chan].wave = 0;
      chan[c.chan].ws.changeWave1( chan[c.chan].wave );
    }
    chan[c.chan].ws.init( ins, 32, 15, chan[c.chan].insChanged );
    chan[c.chan].insChanged = false;
    break;
  }
  case DIV_CMD_NOTE_OFF:
    chan[c.chan].active = false;
    chan[c.chan].keyOff = true;
    chan[c.chan].macroInit( NULL );
    break;
  case DIV_CMD_NOTE_OFF_ENV:
  case DIV_CMD_ENV_RELEASE:
    chan[c.chan].std.release();
    break;
  case DIV_CMD_INSTRUMENT:
    if ( chan[c.chan].ins != c.value || c.value2 == 1 )
    {
      chan[c.chan].ins = c.value;
      chan[c.chan].insChanged = true;
    }
    break;
  case DIV_CMD_VOLUME:
    if ( chan[c.chan].vol != c.value )
    {
      chan[c.chan].vol = c.value;
      if ( !chan[c.chan].std.vol.has )
      {
        chan[c.chan].outVol = c.value;
      }
    }
    break;
  case DIV_CMD_GET_VOLUME:
    if ( chan[c.chan].std.vol.has )
    {
      return chan[c.chan].vol;
    }
    return chan[c.chan].outVol;
    break;
  case DIV_CMD_PITCH:
    chan[c.chan].pitch = c.value;
    chan[c.chan].freqChanged = true;
    break;
  case DIV_CMD_WAVE:
    chan[c.chan].wave = c.value;
    chan[c.chan].ws.changeWave1( chan[c.chan].wave );
    chan[c.chan].keyOn = true;
    break;
  case DIV_CMD_NOTE_PORTA:
  {
    int destFreq = NOTE_FREQUENCY( c.value2 );
    bool return2 = false;
    if ( destFreq > chan[c.chan].baseFreq )
    {
      chan[c.chan].baseFreq += c.value * ( ( parent->song.linearPitch == 2 ) ? 1 : 8 );
      if ( chan[c.chan].baseFreq >= destFreq )
      {
        chan[c.chan].baseFreq = destFreq;
        return2 = true;
      }
    }
    else
    {
      chan[c.chan].baseFreq -= c.value * ( ( parent->song.linearPitch == 2 ) ? 1 : 8 );
      if ( chan[c.chan].baseFreq <= destFreq )
      {
        chan[c.chan].baseFreq = destFreq;
        return2 = true;
      }
    }
    chan[c.chan].freqChanged = true;
    if ( return2 )
    {
      chan[c.chan].inPorta = false;
      return 2;
    }
    break;
  }
  case DIV_CMD_STD_NOISE_MODE:
    chan[c.chan].noise = c.value;
    chan[c.chan].freqChanged = true;
    break;
  case DIV_CMD_PANNING:
    break;
  case DIV_CMD_LEGATO:
    chan[c.chan].baseFreq = NOTE_FREQUENCY( c.value + ( ( HACKY_LEGATO_MESS ) ? ( chan[c.chan].std.arp.val ) : ( 0 ) ) );
    chan[c.chan].freqChanged = true;
    chan[c.chan].note = c.value;
    break;
  case DIV_CMD_PRE_PORTA:
    if ( chan[c.chan].active && c.value2 )
    {
      if ( parent->song.resetMacroOnPorta ) chan[c.chan].macroInit( parent->getIns( chan[c.chan].ins, DIV_INS_PCE ) );
    }
    if ( !chan[c.chan].inPorta && c.value && !parent->song.brokenPortaArp && chan[c.chan].std.arp.will && !NEW_ARP_STRAT ) chan[c.chan].baseFreq = NOTE_FREQUENCY( chan[c.chan].note );
    chan[c.chan].inPorta = c.value;
    break;
  case DIV_CMD_GET_VOLMAX:
    return 63;
    break;
  case DIV_CMD_MACRO_OFF:
    chan[c.chan].std.mask( c.value, true );
    break;
  case DIV_CMD_MACRO_ON:
    chan[c.chan].std.mask( c.value, false );
    break;
  case DIV_CMD_MACRO_RESTART:
    chan[c.chan].std.restart( c.value );
    break;
  default:
    break;
  }
  return 1;
}

void DivPlatformMiniGumby::muteChannel( int ch, bool mute )
{
  isMuted[ch] = mute;
}

void DivPlatformMiniGumby::forceIns()
{
  for ( int i = 0; i < chan.size(); i++ )
  {
    chan[i].insChanged = true;
    chan[i].freqChanged = true;
    updateROMWave( i );
  }
}

void* DivPlatformMiniGumby::getChanState( int ch )
{
  return &chan[ch];
}

DivMacroInt* DivPlatformMiniGumby::getChanMacroInt( int ch )
{
  return &chan[ch].std;
}

unsigned short DivPlatformMiniGumby::getPan( int ch )
{
  return 0;
}

DivDispatchOscBuffer* DivPlatformMiniGumby::getOscBuffer( int ch )
{
  return oscBuf[ch];
}

unsigned char* DivPlatformMiniGumby::getRegisterPool()
{
  minnie->update_reg_pool( regPool );
  return regPool.data();
}

int DivPlatformMiniGumby::getRegisterPoolSize()
{
  return 32;
}

void DivPlatformMiniGumby::reset()
{
  while ( !writes.empty() ) writes.pop();
  std::fill_n( regPool.data(), regPool.size(), 0 );
  for ( int i = 0; i < chan.size(); i++ )
  {
    chan[i] = DivPlatformMiniGumby::Channel();
    chan[i].std.setEngine( parent );
    chan[i].ws.setEngine( parent );
    chan[i].ws.init( NULL, 64, 256, false );
  }
  if ( dumpWrites )
  {
    addWrite( 0xffffffff, 0 );
  }
  minnie->device_start();

  updateROMWaves();
}

int DivPlatformMiniGumby::getOutputCount()
{
  return 1;
}

bool DivPlatformMiniGumby::keyOffAffectsArp( int ch )
{
  return true;
}

void DivPlatformMiniGumby::updateROMWaves()
{
  // copy wavetables
  for ( int i = 0; i < 4; i++ )
  {
    updateROMWave( i );
  }
}

void DivPlatformMiniGumby::updateROMWave( size_t i )
{
  int data = 0;
  DivWavetable* w = parent->getWave( i );

  for ( int j = 0; j < 64; j++ )
  {
    if ( w->max < 1 || w->len < 1 )
    {
      data = 0;
    }
    else
    {
      data = w->data[j * w->len / 64] * 256 / w->max;
      if ( data < 0 ) data = 0;
      if ( data > 255 ) data = 255;
    }
    minnie->update_waveform( i, j, data );
  }
}

void DivPlatformMiniGumby::notifyWaveChange( int wave )
{
  for ( int i = 0; i < chan.size(); i++ )
  {
    if ( chan[i].wave == wave )
    {
      chan[i].ws.changeWave1( wave );
    }
  }
  updateROMWaves();
}

void DivPlatformMiniGumby::notifyInsDeletion( void* ins )
{
  for ( int i = 0; i < chan.size(); i++ )
  {
    chan[i].std.notifyInsDeletion( (DivInstrument*)ins );
  }
}

void DivPlatformMiniGumby::setFlags( const DivConfig& flags )
{
  chipClock = SAMPLING_FREQUENCY;
  CHECK_CUSTOM_CLOCK;
  rate = SAMPLING_FREQUENCY;
  for ( int i = 0; i < oscBuf.size(); i++ )
  {
    oscBuf[i]->setRate( rate );
  }
}

void DivPlatformMiniGumby::poke( unsigned int addr, unsigned short val )
{
  rWrite( addr, val );
}

void DivPlatformMiniGumby::poke( std::vector<DivRegWrite>& wlist )
{
  for ( DivRegWrite& i : wlist ) rWrite( i.addr, i.val );
}

int DivPlatformMiniGumby::init( DivEngine* p, int channels, int sugRate, const DivConfig& flags )
{
  parent = p;
  dumpWrites = false;
  skipRegisterWrites = false;
  for ( int i = 0; i < oscBuf.size(); i++ )
  {
    isMuted[i] = false;
    oscBuf[i] = new DivDispatchOscBuffer;
  }
  minnie = new minnie_device();
  setFlags( flags );
  reset();
  return 6;
}

void DivPlatformMiniGumby::quit()
{
  for ( int i = 0; i < oscBuf.size(); i++ )
  {
    delete oscBuf[i];
  }
  delete minnie;
}

DivPlatformMiniGumby::~DivPlatformMiniGumby()
{
}
