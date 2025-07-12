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

#include "minnie.h"
#include <algorithm>

minnie_device::minnie_device()
{
}

minnie_device::~minnie_device()
{
}

void minnie_device::device_start()
{
  std::fill_n( m_soundregs.data(), m_soundregs.size(), 0 );
  std::fill_n( m_waveram.data(), m_waveram.size(), 0 );

  for ( auto & voice : m_channels )
  {
    voice.frequency = 0;
    voice.index = 0;
    voice.volume_mantissa = 0;
    voice.volume_exponent = 0;
    voice.noise_shift = 16;
    voice.waveform_index = 0;
  }
}

void minnie_device::poke( size_t offset, uint8_t data )
{
  switch ( offset )
  {
  case FREQ1L:
    m_channels[0].frequency = ( m_channels[0].frequency & 0xff00 ) | data;
    break;
  case FREQ1H:
    m_channels[0].frequency = ( m_channels[0].frequency & 0x00ff ) | ( data << 8 );
    break;
  case VOL1:
    m_channels[0].set_volume( data );
    break;
  case TIMBRE1:
    m_channels[0].set_timbre( data );
    break;
  case INDEX1L:
    m_channels[0].index = ( m_channels[0].index & 0xff00 ) | data;
    break;
  case INDEX1H:
    m_channels[0].index = ( m_channels[0].index & 0x00ff ) | ( data << 8 );
    break;
  case FREQ2L:
    m_channels[1].frequency = ( m_channels[1].frequency & 0xff00 ) | data;
    break;
  case FREQ2H:
    m_channels[1].frequency = ( m_channels[1].frequency & 0x00ff ) | ( data << 8 );
    break;
  case VOL2:
    m_channels[1].set_volume( data );
    break;
  case TIMBRE2:
    m_channels[1].set_timbre( data );
    break;
  case INDEX2L:
    m_channels[1].index = ( m_channels[1].index & 0xff00 ) | data;
    break;
  case INDEX2H:
    m_channels[1].index = ( m_channels[1].index & 0x00ff ) | ( data << 8 );
    break;
  case FREQ3L:
    m_channels[2].frequency = ( m_channels[2].frequency & 0xff00 ) | data;
    break;
  case FREQ3H:
    m_channels[2].frequency = ( m_channels[2].frequency & 0x00ff ) | ( data << 8 );
    break;
  case VOL3:
    m_channels[2].set_volume( data );
    break;
  case TIMBRE3:
    m_channels[2].set_timbre( data );
    break;
  case INDEX3L:
    m_channels[2].index = ( m_channels[2].index & 0xff00 ) | data;
    break;
  case INDEX3H:
    m_channels[2].index = ( m_channels[2].index & 0x00ff ) | ( data << 8 );
    break;
  default:
    break;
  }
}

void minnie_device::update_waveform( size_t waveform, size_t offset, uint8_t data )
{
  if ( waveform >= WAVETABLES_COUNT || offset >= WAVETABLE_SIZE )
    return;

  m_waveram[waveform * WAVETABLE_SIZE + offset] = data;
}

int16_t minnie_device::sample_audio( int16_t* chanBuf )
{
  uint16_t noise = next_noise();
  int16_t sample = 0;

  if ( m_sound_enable )
  {
    for ( size_t i = 0; i < m_channels.size(); ++i )
    {
      int16_t voice_sample = m_channels[i].sample_voice( static_cast<int16_t>( noise ), m_waveram.data() );
      chanBuf[i] = voice_sample;
      sample += voice_sample;
    }
  }

  return sample;
}

void minnie_device::sound_enable( int state )
{
  m_sound_enable = state;
}

void minnie_device::update_reg_pool( std::array<unsigned char, 32>& reg )
{
  reg[INDEX1L] = m_channels[0].index & 0xff;
  reg[INDEX1H] = ( m_channels[0].index >> 8 ) & 0xff;
  reg[INDEX2L] = m_channels[1].index & 0xff;
  reg[INDEX2H] = ( m_channels[1].index >> 8 ) & 0xff;
  reg[INDEX3L] = m_channels[2].index & 0xff;
  reg[INDEX3H] = ( m_channels[2].index >> 8 ) & 0xff;
}

uint16_t minnie_device::next_noise()
{
  m_lfsr = m_lfsr >> 1 | ( ( ( m_lfsr >> 3 ) ^ m_lfsr ) << 15 );
  return m_lfsr;
}

void minnie_device::sound_channel::set_volume( uint8_t value )
{
  volume_mantissa = ( 16 | ( ( value >> 1 ) & 7 ) ) << 2;
  volume_exponent = ( value >> 4 ) & 7;
}

void minnie_device::sound_channel::set_timbre( uint8_t value )
{
  waveform_index = value & 7;
  noise_shift = value & 0x80 ? 7 - ( ( value >> 4 ) & 7 ) : ~0;
}

int16_t minnie_device::sound_channel::sample_voice( int16_t noise, uint8_t const* waveform )
{
  index += frequency;
  int32_t modulation = noise_shift == ~0 ? 0 : static_cast<int32_t>( noise ) >> noise_shift;

  int16_t s = sample( index + modulation, waveform );
  int16_t smul = s * volume_mantissa;
  int16_t sexp = smul >> volume_exponent;

  return sexp;
}

int16_t minnie_device::sound_channel::sample( uint16_t index, uint8_t const* waveform ) const
{
  switch ( waveform_index )
  {
  case 0:
    return *( waveform + WAVETABLE_SIZE * 0 + ( index >> 10 ) );
  case 1:
    return *( waveform + WAVETABLE_SIZE * 1 + ( index >> 10 ) );
  case 2:
    return *( waveform + WAVETABLE_SIZE * 2 + ( index >> 10 ) );
  case 3:
    return *( waveform + WAVETABLE_SIZE * 3 + ( index >> 10 ) );
  case 5:
    return sawtooth( index >> 8 );
  case 6:
    return square( index >> 8 );
  case 7:
    return triangle( index >> 8 );
  case 4:
  default:
    return -1;
  }
}

int16_t minnie_device::sound_channel::sawtooth( uint8_t index ) const
{
  return (int8_t)index;
}
int16_t minnie_device::sound_channel::square( uint8_t index ) const
{
  return (int8_t)( index & 0x80 );
}
int16_t minnie_device::sound_channel::triangle( uint8_t index ) const
{
  return (int8_t)( ( ( index << 1 ) ^ ( (int8_t)( index & 0x80 ) >> 7 ) ) + 0x80 );
}

