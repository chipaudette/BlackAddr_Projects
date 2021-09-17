/*
	VoiceToMIDI
	
	Chip Audette (SynthHacker)
	SEPT 2021
	
	*** PURPOSE
	
	Detect the pitch of a voice and convert to MIDI notes
	
	*** CONNECTIONS
	
	Uses the BlackAddr Teensy Guitar Adapter (Teensy 3.6 version)
	Plug in a (dynamic) microphone to the first guitar input.
	
	*** LICENSE
	
	MIT License.  Use at your own risk.  Have fun!

*/
//include libraries
#include <Tympan_Library.h>
#include "AudioRMS_F32.h"
#include "AudioThreshold_F32.h"
#include "AudioVoiceToFreq_F32.h"
#include "Note.h"
#include <BALibrary.h> //for BlackAddr hardware: https://github.com/Blackaddr/BALibrary
#include <BAEffects.h>

// here's where I create the audio classes and audio connections for the signal processing
#include "AudioConnections.h"

// ///////////////////////////////////////// MIDI NOTE STUFF

#define N_MIDI_NOTES 1
Note notes[N_MIDI_NOTES];
float prev_freq_Hz = 0.0;

int freqToNoteNum(float freq_Hz) { 
  //https://newt.phys.unsw.edu.au/jw/notes.html
  return (int)(12.0*log2f(freq_Hz/440)+0.5) + 69 -1;
}

void issueNewNote(int noteNum) {
  notes[0].noteNum = noteNum;
  notes[0].startLegatoNote(); //starts a new note before stopping the previous note (legato)
}

void stopCurrentNote(void) {
  notes[0].stopNote();
}

// ///////////////////////////////////////// BlackAddr hardware stuff

using namespace BALibrary; // This prevents us having to put BALibrary:: in front of all the Blackaddr Audio components
BAAudioControlWM8731 codecControl;

// To get the calibration values for your particular board, first run the
// BAExpansionCalibrate.ino example and 
constexpr int  potCalibMin = 1, potCalibMax = 1023;
constexpr bool potSwapDirection = true;

// Create a control object using the number of switches, pots, encoders and outputs on the
// Blackaddr Audio Expansion Board.
BAPhysicalControls controls(BA_EXPAND_NUM_SW, BA_EXPAND_NUM_POT, BA_EXPAND_NUM_ENC, BA_EXPAND_NUM_LED);

// BAPhysicalControls returns a handle when you register a new control. We'll uses these handles when working with the controls.
//int button1_handle, button2_handle, notenum1_handle, notenum2_handle, velocity_handle, led1_handle, led2_handle; // Handles for the various controls
int button1_handle, button2_handle, pot_handle[3], led1_handle, led2_handle; // Handles for the various controls

void setupKnobsAndButtons(void) {
  // Setup the controls. The return value is the handle to use when checking for control changes, etc.
  // pushbuttons
  button1_handle = controls.addSwitch(BA_EXPAND_SW1_PIN); // will be used for bypass control
  button2_handle = controls.addSwitch(BA_EXPAND_SW2_PIN); // will be used for stepping through filters
  
  // pots...NOTE: pot3 is actually physically between pot1 and pot2, so let's do note order as 0,2,1
  pot_handle[0] = controls.addPot(BA_EXPAND_POT1_PIN, potCalibMin, potCalibMax, potSwapDirection); // control the amount of delay
  pot_handle[2] = controls.addPot(BA_EXPAND_POT2_PIN, potCalibMin, potCalibMax, potSwapDirection); 
  pot_handle[1] = controls.addPot(BA_EXPAND_POT3_PIN, potCalibMin, potCalibMax, potSwapDirection); 
  
  // leds
  led1_handle = controls.addOutput(BA_EXPAND_LED1_PIN);
  led2_handle = controls.addOutput(BA_EXPAND_LED2_PIN); // will illuminate when pressing SW2
}

void setupAICconfiguration(void) {
  //gain settings can be values of 0 to 31, with 23 being 0 dB of gain
  //codecControl.setLeftInputGain(23);  // Span 0-31 in 1.5dB steps. 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setLeftInputGain(28);  // Span 0-31 in 1.5dB steps. 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setRightInputGain(23); // Span 0-31 in 1.5dB steps. 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setLeftInMute(false);  //make sure not muted
  codecControl.setRightInMute(false); //not muted
  codecControl.setHPFDisable(false);  // Start with the HPF enabled.
  delay(100);

  codecControl.recalibrateDcOffset();
}

// ////////////////////////////////////////////// Arduino setup() and loop()

//Create setup() and loop() functions, per Arduino standard
elapsedMillis timer, timer2;
void setup(void) {
  Serial.begin(115200);  //serial to the PC via USB...unnecessary on Teensy 3.x and 4.x
  Serial1.begin(31250);  //serial for MIDI
  delay(1000);
  Serial.println("Vocoder_viaBPFilters: Starting...");
  
  //TGA_PRO_MKII_REV1(); // Declare the version of the TGA Pro you are using.
  TGA_PRO_REVB(x);  //or TGA_PRO_REVA(x);

  //make the audio connections
  int n_connections = makeConnections();
  Serial.println("setup(): made " + String(n_connections) + " audio connections");

  delay(5); // wait a few ms to make sure the GTA Pro is fully powered up
  AudioMemory(50); // Provide an arbitrarily large number of audio buffers (48 blocks) for the effects
  AudioMemory_F32(50);

  // If the codec was already powered up (due to reboot) power it down first
  codecControl.disable();  delay(100);
  codecControl.enable();  delay(100);

  //setup AIC configuration
  setupAICconfiguration();

  //setup the knobs and buttons
  setupKnobsAndButtons();

  //setup signal processing
  setupSignalProcessing(); //this is in AudioConnections.h

  notes[0].enabled=true; //allow notes to start flowing
  Serial.println("setup() complete...");
}


bool debug_isPlaying=false;
void loop(void) {  

  //check the frequency
  float cur_freq_Hz = voiceToFreq.getFrequency_Hz();
  float new_note_num = notes[0].noteNum;
  if (fabs(cur_freq_Hz - prev_freq_Hz) > 0.1) {
    new_note_num = freqToNoteNum(cur_freq_Hz);
    if (new_note_num != notes[0].noteNum) {
      issueNewNote(new_note_num);
    }
  }

  //turn note on, if warranted
  if ((notes[0].is_active == false) && (voiceDetect.getValue() >= 0.5)) {
    issueNewNote(notes[0].noteNum);
  }
  if ((notes[0].is_active == true) && (voiceDetect.getValue() < 0.5)) {
    stopCurrentNote();
  }

  //service knobs, buttons, LEDs
  serviceKnobsAndButtons();
  serviceLEDs();

//  if (timer > 1000) {
//    if (debug_isPlaying == false) {
//      Serial.println("Start note");
//      Serial1.write((byte)0x90 + (byte)1);Serial1.write((byte)69);Serial1.write((byte)127);
//      debug_isPlaying = true;
//    } else {
//      Serial.println("Stop note");
//      Serial1.write((byte)0x80 + (byte)1);Serial1.write((byte)69);Serial1.write((byte)127);
//      debug_isPlaying = false;
//    }
//    timer = 0; 
//  }



#if 0
  if (timer > 50) {
    timer = 0;
    Serial.println(String(rms_lp.get_rms_dBFS(),0) + "," + String(rms_hp.get_rms_dBFS(),0) + ", " + String(rms_lp.get_rms_dBFS()-rms_hp.get_rms_dBFS(),0) );
  }
#endif

#if 0
  if (timer > 25) {
    timer = 0;
    Serial.print(voiceToFreq.getFrequency_Hz());
    Serial.print(", ");
    Serial.print(max(0.0,(rms.get_rms_dBFS()+90)*4));
    Serial.println();
  }
#endif

#if 1
  if (timer2 > 3000) {
    timer2 = 0;
    Serial.print("Processor Usage (%): "); Serial.print(AudioProcessorUsage());
    Serial.print(", MEM Usage: ");Serial.print(AudioMemoryUsage_F32());
    Serial.println();
 }
 #endif

  delay(20); // Without some minimal delay here it will be difficult for the pots/switch changes to be detected.
}


// ////////////////////////////// servicing routines

void serviceKnobsAndButtons(void) {
  float potValue;

  //service the three pots
  for (int i=0; i< 3; i++) {
    if (controls.checkPotValue(pot_handle[i], potValue)) { //returns true if value has changed
      /*
      int new_val = chooseNoteNum(potValue,centerNotes[i]);
      if (new_val != notes[i].noteNum) {
        notes[i].noteNum = new_val;
        notes[i].startNote();  //play an example of the note
        Serial.println("New NoteNum[" + String(new_val) + "] = " + String(new_val)); 
      }
      */
    }
  }

  // Check if SW1 has been toggled (pushed)
  if (controls.isSwitchToggled(button1_handle)) {
    /*
    int ret_val =  notes[0].startNote();
    Serial.print("Started note "); Serial.println(ret_val);
    */
  }

  // Check if SW2 has been toggled (pushed)
  if (controls.isSwitchToggled(button2_handle)) {
    /*
    int ret_val = notes[1].startNote();
    Serial.print("Started note "); Serial.println(ret_val);
    */
  }
}

void serviceLEDs(void) {
   //update LED stats
   controls.setOutput(led1_handle, notes[0].is_active); // Set the LED when NOT bypassed  

}
