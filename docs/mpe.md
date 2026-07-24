# MPE Support

## Implemented Foundation

The voice scheduler already identifies notes by MIDI channel and note number. Note-off, sustain, pitch bend, CC74, channel pressure, and polyphonic aftertouch are routed without allocation on the audio thread. The arpeggiator preserves source channel and velocity.

Formal lower/upper-zone configuration and master/member routing are implemented by the fixed-capacity `GlieseMPEZone` state used by `SynthEngine`.

## Required Behavior

GLIESE 710 supports one active MPE zone:

- **Lower zone:** channel 1 is the master; member channels begin at channel 2.
- **Upper zone:** channel 16 is the master; member channels descend from channel 15.
- Zone membership is configured by MIDI RPN 6 (`MPE Configuration Message`) on the appropriate master channel.
- Sending RPN 6 with zero member channels disables that zone.
- The most recently configured non-empty zone becomes active.

Expression routing:

| MIDI data | Master channel | Member channel |
|---|---|---|
| Pitch bend | Applies to every voice in the zone | Applies only to that member-channel voice |
| CC74 | Applies to every voice in the zone | Applies only to that member-channel voice |
| Channel pressure | Applies to every voice in the zone | Applies only to that member-channel voice |
| Sustain CC64 | Applies to every voice/note in the zone | Applies to that member channel |
| Note on/off | Not used for member notes | Channel-per-note voice identity |

Polyphonic key pressure continues to target the exact channel/note pair when received.

## RPN State

RPN parsing must use fixed per-channel state:

- CC101: RPN MSB
- CC100: RPN LSB
- CC6: Data Entry MSB
- CC38: Data Entry LSB where applicable
- RPN Null (`127/127`) clears the selection

No dynamic containers, logging, callbacks, or host notifications are allowed in render-path MIDI handling.

## Pitch-Bend Range

The existing `master_pb_range` parameter remains the synth-side bend range. A future upgrade may interpret per-zone pitch-bend-range RPN messages and expose separate master/member bend ranges. Until then, controllers should be configured to match the patch range.

## Arpeggiator Policy

The arpeggiator preserves member channels and velocities. Master-channel expression continues to affect generated member notes. Latch and sorted patterns must keep channel/note identity distinct when equal note numbers arrive on different member channels.

## Tests

Regression coverage includes:

- Lower- and upper-zone RPN 6 configuration
- Zone disable through zero members
- Master/member expression routing rules
- Arpeggiator channel/velocity preservation
- Dense render-path MIDI and DSP performance coverage

Processor-level voice-state tests for master sustain release and identical notes on multiple member channels remain desirable as the processor test harness expands.

## Later Extensions

- User-facing lower/upper-zone configuration and status
- Separate master/member pitch-bend ranges
- MIDI 2.0 per-note controller mapping
- Per-note expression visualization in the voice/CPU meter
