# ES7210 Microphone Array Configuration

Documentation of the microphone subsystem on the Waveshare ESP32-S3-RLCD-4.2 board.

## Overview

The board uses a **dual microphone voice recognition array** with acoustic echo cancellation (AEC). This is NOT a stereo audio capture system - it's designed for speech recognition and voice interaction.

## Hardware Configuration

### Physical Microphones

Two digital MEMS microphones (MIC1 and MIC2) with 4-pin interface:
- VDD (power)
- GND (ground x2)
- DAT (digital data output)

Both share bias voltage from `ADC_MICBIAS12` (~2.87V per schematic).

### ES7210 4-Channel ADC

The ES7210 is a high-performance 4-channel audio ADC. On this board:

| TDM Slot | ES7210 Channel | ES7210 Pins | Signal Source | Purpose |
|----------|----------------|-------------|---------------|---------|
| Slot 0 | MIC1 (Ch1) | 15-16 (MIC1N/MIC1P) | Physical MIC1 | Voice capture |
| Slot 1 | MIC2 (Ch2) | 19-20 (MIC2N/MIC2P) | Physical MIC2 | Voice capture |
| Slot 2 | MIC3 (Ch3) | 31-32 (MIC3N/MIC3P) | AEC circuit | Speaker reference |
| Slot 3 | MIC4 (Ch4) | 27-28 (MIC4N/MIC4P) | AEC circuit | Speaker reference |

### Acoustic Echo Cancellation (AEC) Circuit

Channels 3 and 4 are **not connected to microphones**. They receive the speaker output (OUTP/OUTN from ES8311 codec) through:
- 150KΩ resistor dividers
- RC filtering (22pF, 0.1µF capacitors)

This allows DSP algorithms to subtract speaker audio from the microphone signal, enabling voice capture even while the speaker is playing audio.

## I2C Configuration

- **ES7210 I2C Address**: 0x40
- **I2C Pins**: GPIO13 (SDA), GPIO14 (SCL)

## I2S/TDM Configuration

```
MCK  = GPIO16  (Master Clock)
BCK  = GPIO9   (Bit Clock)
WS   = GPIO45  (Word Select / LRCK)
DIN  = GPIO10  (Data In from ES7210)
```

The ES7210 operates in TDM slave mode with 4 slots, 32-bit samples.

## Observed Behavior

When reading MIC1 and MIC2 as separate channels:
- **MIC1**: Low RMS values (~0-20)
- **MIC2**: Higher/noisier RMS values (~60-150)

### Possible Causes

1. **Array Design Intent** - Voice recognition arrays often position mics with different orientations:
   - One toward the user (primary)
   - One for ambient noise reference (for beamforming/noise cancellation)

2. **Physical Placement** - Mics may be intentionally asymmetric for directional pickup

3. **Hardware Issue** - MIC1 could have:
   - Bad solder joint
   - Damaged MEMS element
   - ADC channel issue

## Recommendations for Different Use Cases

### VU Meter / Audio Level Display

**Recommended: Single combined meter**
```cpp
float combined_rms = sqrtf((rms_mic1 * rms_mic1 + rms_mic2 * rms_mic2) / 2.0f);
```

Or use weighted combination biased toward the stronger mic:
```cpp
float combined_rms = 0.2f * rms_mic1 + 0.8f * rms_mic2;
```

### Voice Activity Detection

Sum both channels - the array design means both mics contribute to voice detection even if one is weaker.

### Speech Recognition

Use both MIC1 and MIC2 data. Advanced implementations can use the differential signal for:
- Beamforming (directional pickup)
- Noise cancellation
- Far-field voice capture

### Echo Cancellation

Read all 4 TDM slots. Channels 3-4 provide the speaker reference signal needed to subtract speaker audio from the mic input.

## ES7210 Register Reference

Key registers used in initialization:

| Register | Address | Purpose |
|----------|---------|---------|
| RESET | 0x00 | Soft reset control |
| CLK_ON | 0x01 | Clock gating |
| MODE_CTL | 0x08 | Master/slave mode |
| SDP_CFG2 | 0x12 | TDM mode enable |
| MIC12_BIAS | 0x41 | Mic 1/2 bias voltage |
| ADC1_GAIN | 0x43 | Channel 1 gain (0-37.5dB) |
| ADC2_GAIN | 0x44 | Channel 2 gain |
| ADC3_GAIN | 0x45 | Channel 3 gain |
| ADC4_GAIN | 0x46 | Channel 4 gain |

Gain values: Each step is 0.5dB. Value 0x1A = 30dB gain.

## References

- [Waveshare ESP32-S3-RLCD-4.2 Product Page](https://www.waveshare.com/esp32-s3-rlcd-4.2.htm)
- [Waveshare Documentation](https://docs.waveshare.com/ESP32-S3-RLCD-4.2)
- [ES7210 Datasheet](https://files.waveshare.com/wiki/common/ES7210-datasheet.pdf)
- [ESPHome ES7210 Component](https://esphome.io/components/audio_adc/es7210/)
- Local schematic: `REFERENCES/ESP32-S3-RLCD-4.2-schematic.pdf`

## Future Work

- [ ] Investigate MIC1 hardware (probe with oscilloscope, check solder joints)
- [ ] Implement combined VU meter using both channels
- [ ] Add speech-to-text using cloud API (WiFi required)
- [ ] Explore ESP-SR for on-device wake word detection
- [ ] Implement AEC for voice capture during speaker playback
