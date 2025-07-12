#ifndef MINNIE_H
#define MINNIE_H

#include <cstdint>
#include <memory>
#include <array>

class minnie_device
{
public:

  static constexpr const size_t WAVETABLE_SIZE = 64;
  static constexpr const size_t WAVETABLES_COUNT = 4;
  static constexpr const size_t VOICES = 3;
  static constexpr const size_t SOUND_REGS = 32;

  static constexpr const size_t FREQ1L  = 0x00;
  static constexpr const size_t FREQ1H  = 0x01;
  static constexpr const size_t VOL1    = 0x02;
  static constexpr const size_t TIMBRE1 = 0x03;
  static constexpr const size_t INDEX1L = 0x04;
  static constexpr const size_t INDEX1H = 0x05;

  static constexpr const size_t FREQ2L  = 0x08;
  static constexpr const size_t FREQ2H  = 0x09;
  static constexpr const size_t VOL2    = 0x0a;
  static constexpr const size_t TIMBRE2 = 0x0b;
  static constexpr const size_t INDEX2L = 0x0c;
  static constexpr const size_t INDEX2H = 0x0d;

  static constexpr const size_t FREQ3L  = 0x10;
  static constexpr const size_t FREQ3H  = 0x11;
  static constexpr const size_t VOL3    = 0x12;
  static constexpr const size_t TIMBRE3 = 0x13;
  static constexpr const size_t INDEX3L = 0x14;
  static constexpr const size_t INDEX3H = 0x15;


  struct sound_channel
  {
    uint16_t frequency;
    uint16_t index;
    int16_t volume_mantissa;
    int16_t volume_exponent;
    uint16_t noise_shift;
    uint16_t waveform_index;

    void set_volume( uint8_t value );
    void set_timbre( uint8_t value );

    int16_t sample_voice( int16_t noise, uint8_t const* waveform );

  private:
    int16_t sample( uint16_t index, uint8_t const* waveform ) const;
    int16_t sawtooth( uint8_t index ) const;
    int16_t square( uint8_t index ) const;
    int16_t triangle( uint8_t index ) const;
  };

  minnie_device();
  ~minnie_device();

  // device-level overrides
  void device_start();
  void poke( size_t offset, uint8_t data );
  void update_waveform( size_t waveform, size_t offset, uint8_t data );
  int16_t sample_audio( int16_t* chanBuf );
  void sound_enable( int state );
  void update_reg_pool( std::array<unsigned char, 32> & reg );

private:
  uint16_t next_noise();

  std::array<sound_channel, VOICES> m_channels = {};
  std::array<uint8_t, WAVETABLE_SIZE * WAVETABLES_COUNT> m_waveram;
  std::array<uint8_t, SOUND_REGS> m_soundregs;
  uint16_t m_lfsr = 1;
  bool m_sound_enable = true;
};

#endif // MINNIE_H
