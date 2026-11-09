# Device test: one MIDI Note On per sequencer hit

Notes and results for `claude/seq-single-note-on`, tested on an LXR-02 on
2026-11-09. Written to be parked: it records what was shown, what was not, and
what is left.

## The problem

`seq_triggerVoice()` (sequencer.c) sent two Note Ons per hit: one from
`voiceControl_noteOn()` (the step's note) and one of its own (the voice's MIDI
note setting, `PAR_MIDI_NOTE1-7`, if set). `seq_sendMidiNoteOn()` remembers one
note per channel, so only the second gets a Note Off. The fix adds a `sendMidi`
argument to `voiceControl_noteOn()`: the sequencer passes 0, notes received over
MIDI pass 1.

## Setup

- LXR-02, MIDI channel 1, BPM set to **MID** (external clock).
- Roland UM-ONE: UM-ONE OUT -> LXR MIDI IN, LXR MIDI OUT -> UM-ONE IN.
- Four steps on voice 1, no steps on other tracks (kept in RAM, not saved).
  Steps on other tracks add a burst of notes at the first step.
- A script acts as clock master: it sends MIDI Start and 24 ppqn clock
  (2 bars at 120 bpm) and records the notes the LXR sends back. A hit is a
  group of Note Ons within 5 ms. The script is not in this repo.
- Firmware is installed from the SD card: hold Data (encoder), power on, wait
  for "Update OK!", then power cycle without the knob. Only one `LXR*.img` may
  be on the card.

## Results

| Firmware | Hits | Note Ons per hit | Note 63 (step's note) | Note 48 (voice's MIDI note) |
|---|---|---|---|---|
| Source v1.7, no fix (`main` + test setup) | 8 | **2** (all 8) | **8 on / 0 off** | 8 on / 8 off |
| Source v1.7 + this fix | 8 | **1** (all 8) | none | 8 on / 8 off |

- The "before" capture was also run twice on an earlier pattern (steps on
  several tracks): 7 normal hits were pairs (63 then 48) and note 63 had 14 Ons
  and 0 Offs. Same result.
- Echo check (a note played into the LXR over MIDI is still sent out): Note On
  48 on channel 1 gives Note On 60, Note Off 60, Note On 60 (velocity 100), the
  same before and after the fix. A voice whose MIDI note setting matches plays
  the default note 60. Incoming Note Offs are ignored by the parser.

Image checksums (SHA-256):

- no fix: `ac767f740b6112e08f6de175d4ed6db29f39f831d456a3c69cd5717271cbea7f`
- with fix: `d300222f3cd6c1ab90421c583059f55c7f4571227505d4124702552aaddf8f7e`

## Not shown

- **Stock v1.71.** The unit shipped with v1.71 (its image identifies itself as
  "LXR2 Drums V 1.71"); the repo source is labelled 1.7. On stock v1.71 the
  same capture recorded Note Offs at each hit but **no Note Ons at all**, in two
  runs, and the echo check returned only a Note Off. Not explained: the source
  would not send an Off without a Note On first. Not checked: the unit's note TX
  setting after flashing (Shift+Config). So the double Note On is shown for the
  published source, not for the shipped v1.71 binary.
- Hanging notes on a real receiver: shown as Note Ons without Offs on the wire,
  not as a stuck note on a synth.
- Only voice 1 and a MIDI note setting of 48 on the device. Other voices and the
  "MIDI note = 0" case are covered by `tests/test_sequencer_midi_notes.c` only.
  USB MIDI out was not tested.
- Voices that share a MIDI channel still leave earlier voices' notes without
  Offs (one remembered note per channel). Separate from this fix.

## Status (parked 2026-11-09)

- Branch `claude/seq-single-note-on`, local only, not pushed, no PR open.
- Draft PR description is ready (outside the repo), device results filled in.
  Add a line about the stock v1.71 result and ask which commit v1.71 was built
  from.
- PR #10 (CC 0 wrap) is open; this branch carries the same test-setup commit.
- The unit is running stock v1.71 (as it came).
- To do: optionally check note TX on the global page and rerun on stock v1.71;
  then, on go-ahead, push to the fork and open the PR. Drop this file from the
  PR if it should not be part of it.
