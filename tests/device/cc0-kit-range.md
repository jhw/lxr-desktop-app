# Device test: MIDI bank select (CC 0) past the last kit

Checks that CC 0 values 64-127 no longer load a kit. There are only 64 kits
(0-63). Before the fix, the values 64-127 made the LXR copy "a kit" from past
the end of its kit memory.

Cannot be unit tested end to end: the unit test (`test_frontpanel_cc`) covers
the parser, this covers the real hardware.

## What you need
- An LXR-02, an SD card, and a way to send a single MIDI CC (a DAW, a
  hardware sequencer, or a MIDI utility) over DIN or USB.
- Two firmware images: one built **without** the fix (`main`) and one **with**
  it. Build with `cmake --preset release && cmake --build --preset release`
  and use `build/release/LXR_V2_V1.7.img`.
- Back up the SD card first. The test shouldn't write to it, but the
  "without the fix" firmware loads garbage as a kit.

## Setup (both firmware versions)
1. Update the firmware (put the `.img` on the SD card, hold the encoder
   button while switching on, wait for `Update OK!`).
2. In the MIDI settings, check that the LXR receives CC on the channel you
   will send on (the CC filter must be on).
3. Don't change any kit after this, and don't save.
4. On the Load/Save page, select a kit (for example kit 5) and note the kit
   number and how it sounds. Play a pattern with a few voices so you can hear
   changes.

## A. Without the fix (shows the problem)
1. Send CC 0 value 3, then value 10. Each time the kit should change and the
   sound should change to that of the stored kit. This part already works.
2. Send CC 0 value 64. Then 100, then 127.
3. Note what happens. Expected without the fix: the sound or parameters
   change to something that isn't one of your kits (random parameter values,
   odd sounds, possibly a hang or a reset). The exact result depends on what
   follows the kit memory, so write down what you see, not just "it changed".
4. Power-cycle before the next step.

## B. With the fix (shows it is fixed)
1. Update to the fixed firmware and repeat the setup.
2. Send CC 0 values 0, 1, 63. Each should load that kit, as before.
3. Send CC 0 value 64, 100 and 127. Nothing should happen: the sound and all
   parameters stay as they were, and the next value 0-63 still loads that kit.
4. Send CC 0 value 63 then 64: the kit loaded by 63 stays loaded.

## Record
Firmware (commit), MIDI source, channel, the kit numbers you tried, and what
you saw in A.3 and B.3.
