/*
 * test_sequencer_midi_notes.c - the MIDI notes the sequencer sends when it
 * plays a step (seq_triggerVoice() in sequencer.c, voiceControl_noteOn() in
 * MidiVoiceControl.c)
 *
 * Copyright (C) 2026 Erica Synths
 * SPDX-License-Identifier: GPL-3.0-only
 *
 * This file is part of the LXR firmware. It is licensed under the GNU General
 * Public License version 3, with the additional permission and terms in
 * NOTICE.md. See LICENSE and NOTICE.md in the root of the repository.
 */

#include "lxr_test.h"
#include "Soundengine/Sequencer/sequencer.h"
#include "Soundengine/MIDI/MidiVoiceControl.h"
#include "Soundengine/MIDI/MidiParser.h"
#include "IO/uart.h"
#include "menu/menu.h"
#include "Preset/PresetManager.h"
#include "ledHandler.h"
#include "buttonHandler.h"
#include "Soundengine/DSPAudio/automationNode.h"
#include "Soundengine/FxBus.h"

// --- Variables the code under test needs --------------------------------

uint8_t activeParameters[NUM_PARAMS];
uint8_t frontParser_activeTrack;
uint8_t frontParser_shownPattern;
FxBus fxBus;
uint8_t menu_activeBar;
uint8_t menu_activePage;
uint8_t menu_loadedPresetNr[NUM_CURRENT];
uint8_t menu_playedPattern;
uint8_t menu_selectedPresetNr[NUM_PRESET_LOCATIONS];
uint8_t menu_shownPattern;
uint8_t menu_songMenuPosition;
uint8_t midiParser_txRxFilter;
uint8_t midi_MidiChannels[8];
uint8_t midi_process_block;
uint32_t sync_delayedStepTimer;
volatile uint32_t systick_ticks;
uint8_t trigger_clockReceived;

// --- Stubs: sound engine, front panel, display ------------------------------

uint32_t GetRngValue() { return 0; }
void autoNode_init(AutomationNode *node) {}
void autoNode_setDestination(AutomationNode *node, uint16_t dest) {}
void autoNode_updateValue(AutomationNode *node, uint8_t val) {}
uint8_t buttonHandler_getMode() { return 0; }
void led_blinkHandler() {}
void led_clearActive_step() {}
void led_clearAllBlinkLeds() {}
void led_clearSequencerLeds() {}
uint8_t led_idx2StepLed(uint8_t idx) { return idx; }
void led_initPatternSelectLeds() {}
void led_resetBlinkTimer() {}
void led_setActive_step(uint8_t stepNr) {}
void led_setBlinkLed(const uint8_t ledNr, const uint8_t onOff) {}
void led_setValue(uint8_t val, uint8_t ledNr) {}
void led_updateSeqLeds() {}
void lfo_recalcSync() {}
uint8_t menu_getActiveVoice() { return 0; }
uint8_t menu_getViewedPattern() { return 0; }
void menu_repaint() {}
void menu_repaintAll() {}
void menu_setShownPattern(uint8_t patternNr) {}
void midiParser_checkMtc() {}
void preset_loadDrumsetRam(uint8_t presetNr, uint8_t isMorph, uint8_t sendImmediate) {}
void preset_touchTimer(PresetTimer type) {}
void sync_tick() {}
void trigger_clockTick(uint8_t onOff, uint8_t cnt) {}
void trigger_reset(uint8_t value) {}
void uart_sendFrontpanelByte(uint8_t data) {}
void uart_sendFrontpanelSysExByte(uint8_t data) {}

// The voices: only count that they were triggered.
static int voiceTriggers;
void Drum_trigger(const uint8_t voiceNr, const uint8_t vol, const uint8_t note) { voiceTriggers++; }
void Snare_trigger(const uint8_t vel, const uint8_t note) { voiceTriggers++; }
void Cymbal_trigger(const uint8_t vel, const uint8_t note) { voiceTriggers++; }
void HiHat_trigger(uint8_t vel, uint8_t isOpen, const uint8_t note) { voiceTriggers++; }

// --- MIDI output: record what the sequencer sends -----------------------------

#define MAX_SENT 32
static MidiMsg sent[MAX_SENT];
static int numSent;
static int usbSent;

void uart_sendMidi(MidiMsg msg)
{
	if (numSent < MAX_SENT)
		sent[numSent++] = msg;
}
void usb_sendMidi(MidiMsg msg) { usbSent++; }

// --- Helpers ----------------------------------------------------------------

#define CHANNEL_OF(voice) (voice)

static void reset(uint8_t noteOverride)
{
	midiParser_txRxFilter = 0xff;               // note transmit on
	for (int v = 0; v < 8; v++)
		midi_MidiChannels[v] = (uint8_t)CHANNEL_OF(v);
	for (int v = 0; v < 7; v++)
		activeParameters[PAR_MIDI_NOTE1 + v] = noteOverride;
	// velocity of the step that seq_triggerVoice() sends
	for (int v = 0; v < 7; v++)
		seq_patternSet.seq_subStepPattern[0][v][0].volume = 100;

	// Nothing playing and no notes on. This sends Note Offs for notes left on
	// by the previous test, so clear what was recorded afterwards.
	voiceControl_noteOff(0xff);
	numSent = usbSent = voiceTriggers = 0;
	memset(sent, 0, sizeof sent);
}

static int countOn(int note)
{
	int n = 0;
	for (int i = 0; i < numSent; i++)
		if ((sent[i].status & 0xf0) == NOTE_ON && sent[i].data1 == note && sent[i].data2 > 0)
			n++;
	return n;
}
static int countOff(int note)
{
	int n = 0;
	for (int i = 0; i < numSent; i++)
		if ((sent[i].status & 0xf0) == NOTE_OFF && sent[i].data1 == note)
			n++;
	return n;
}
static int countAllOn(void)
{
	int n = 0;
	for (int i = 0; i < numSent; i++)
		if ((sent[i].status & 0xf0) == NOTE_ON)
			n++;
	return n;
}

// --- Tests --------------------------------------------------------------------

// Default settings: the voice's MIDI note is 48 and the step's note is 60.
// One hit must send one Note On (the override note), not both.
static void test_one_hit_sends_one_note_on_with_a_note_override(void)
{
	reset(48);
	seq_triggerVoice(0, 100, 60);
	CHECK_EQ(voiceTriggers, 1);                 // the voice still plays
	CHECK_EQ(countAllOn(), 1);
	CHECK_EQ(countOn(48), 1);
	CHECK_EQ(countOn(60), 0);
	CHECK_EQ(sent[0].status & 0x0f, CHANNEL_OF(0));
}

// No override (0): the step's own note is sent, once.
static void test_one_hit_sends_one_note_on_without_a_note_override(void)
{
	reset(0);
	seq_triggerVoice(0, 100, 60);
	CHECK_EQ(voiceTriggers, 1);
	CHECK_EQ(countAllOn(), 1);
	CHECK_EQ(countOn(60), 1);
}

// Every note that is turned on must be turned off by the next hit, so nothing
// is left hanging on the receiving instrument.
static void test_every_note_on_gets_a_note_off(void)
{
	reset(48);
	seq_triggerVoice(0, 100, 60);
	seq_triggerVoice(0, 100, 60);
	CHECK_EQ(countAllOn(), 2);
	CHECK_EQ(countOff(48), 1);                  // before the second hit
	CHECK_EQ(countOn(60), 0);                   // no note that never gets an off
	seq_triggerVoice(1, 100, 62);               // another voice: its own channel
	CHECK_EQ(sent[numSent - 1].status & 0x0f, CHANNEL_OF(1));
}

// All 7 voices behave the same, each on its own channel.
static void test_all_voices_send_one_note_on(void)
{
	for (int voice = 0; voice < 7; voice++) {
		reset(48);
		seq_triggerVoice((uint8_t)voice, 100, 60);
		CHECK_EQ(countAllOn(), 1);
		CHECK_EQ(sent[0].status & 0x0f, CHANNEL_OF(voice));
	}
}

// Notes that arrive over MIDI are still sent on, as before.
static void test_incoming_note_is_still_sent_out(void)
{
	reset(48);
	voiceControl_noteOn(2, 64, 90, 1);
	CHECK_EQ(voiceTriggers, 1);
	CHECK_EQ(countAllOn(), 1);
	CHECK_EQ(countOn(64), 1);
	CHECK_EQ(sent[0].status & 0x0f, CHANNEL_OF(2));
}

// With the MIDI send off the voice plays but nothing is sent.
static void test_note_on_without_midi_sends_nothing(void)
{
	reset(48);
	voiceControl_noteOn(2, 64, 90, 0);
	CHECK_EQ(voiceTriggers, 1);
	CHECK_EQ(numSent, 0);
}

int main(void)
{
	RUN_TEST(test_one_hit_sends_one_note_on_with_a_note_override);
	RUN_TEST(test_one_hit_sends_one_note_on_without_a_note_override);
	RUN_TEST(test_every_note_on_gets_a_note_off);
	RUN_TEST(test_all_voices_send_one_note_on);
	RUN_TEST(test_incoming_note_is_still_sent_out);
	RUN_TEST(test_note_on_without_midi_sends_nothing);
	return TEST_REPORT();
}
