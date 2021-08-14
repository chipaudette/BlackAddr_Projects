/*
	BeatBox2MIDI_3Note
	
	Chip Audette (SynthHacker)
	August 2021
	
	*** PURPOSE
	
	Create MIDI notes by "beat boxing" into a microphone.
	This version is sensitive to three types of sounds:
	    * Low pitched: such as a kick
		* Mid-pitched: such as a snare/clap
		* High-pitched: such as a hi-hat
	You can, of course, map these sounds to any MIDI note.
	
	*** CONNECTIONS
	
	Uses the BlackAddr Teensy Guitar Adapter (Teensy 3.6 version)
	Plug in a (dynamic) microphone to the first guitar input.
	Plug in a drum machine (MIDI CHAN 10) to the MIDI Out.
	
	*** LICENSE
	
	MIT License.  Use at your own risk.  Have fun!

*/

#define N_CHAN (3) //how many sounds will the system make?

//include libraries
#include <Audio.h>     //installed along with Teensyduino
#include "AudioRMS.h"
#include "AudioThresholdCompare.h"
#include "AudioTriggerMIDI_3Chan.h"
#include "BALibrary.h" //for BlackAddr hardware: https://github.com/Blackaddr/BALibrary
#include "BAEffects.h"

// here's where I define some important stuff for myself 
#define MIDI_SEND_CHANNEL 10
#include "Note.h"  //uses MIDI_SEND_CHANNEL
Note notes[N_CHAN];

// here's where I create the audio classes and audio connections for the signal processing
#include "AudioConnections.h"

//variables for interpreting some user conrols
const int centerNotes[N_CHAN] = {36, 39, 42}; //kick, clap, closed hi-hat
const int minGain = 0, midGain = 23, maxGain = 31; //for the gain knob (if implemented).  23 is 0dB of gain for the WM8731 codec

// ///////////////////////////////////////// MIDI NOTE STUFF

int chooseNoteNum(float pot_val, int centerValue) { //pot_val is 0.0 to 1.0
  const int offset = 0;  //was 36.  Now, this ends up being built into "centerValue"
  const int width = 13;  //how many semitones should the knob span?  was 16.  dropped to 14
  return (int)(width*(pot_val-0.5) + 0.5)+offset+centerValue; //the first 0.5 is to make centerValue be the center.  The second 0.5 is make it round, not floor
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
int button1_handle, button2_handle, pot_handle[N_CHAN], led1_handle, led2_handle; // Handles for the various controls

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
  codecControl.setLeftInputGain(midGain);  // Span 0-31 in 1.5dB steps. 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setRightInputGain(midGain); // Span 0-31 in 1.5dB steps. 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setLeftInMute(false);  //make sure not muted
  codecControl.setRightInMute(false); //not muted
  codecControl.setHPFDisable(false);  // Start with the HPF enabled.
  delay(100);

  codecControl.recalibrateDcOffset();
}

// ////////////////////////////////////////////// Arduino setup() and loop()

//Create setup() and loop() functions, per Arduino standard
elapsedMillis timer;
void setup(void) {
  Serial.begin(115200);  //serial to the PC via USB
  Serial1.begin(31250);  //serial for MIDI
  delay(500);
  Serial.println("BeatBox2MIDI_Compare3: Starting...");
  
  //TGA_PRO_MKII_REV1(); // Declare the version of the TGA Pro you are using.
  TGA_PRO_REVB(x);  //or TGA_PRO_REVA(x);

  delay(5); // wait a few ms to make sure the GTA Pro is fully powered up
  AudioMemory(100); // Provide an arbitrarily large number of audio buffers (48 blocks) for the effects

  // If the codec was already powered up (due to reboot) power it down first
  codecControl.disable();  delay(100);
  codecControl.enable();  delay(100);

  //setup AIC configuration
  setupAICconfiguration();

  //setup the knobs and buttons
  setupKnobsAndButtons();

  //setup signal processing
  setupSignalProcessing(); //this is in AudioConnections.h

  //initialize the noteNum
  for (int i=0; i<N_CHAN; i++) notes[i].noteNum = centerNotes[i];

  Serial.println("setup() complete...");
}


void loop(void) {  
  serviceKnobsAndButtons();
  serviceLEDs();
  serviceNotes();

#if 0
  if (timer > 50) {
    timer = 0;
    Serial.println(String(rms_lp.get_rms_dBFS(),0) + "," + String(rms_hp.get_rms_dBFS(),0) + ", " + String(rms_lp.get_rms_dBFS()-rms_hp.get_rms_dBFS(),0) );
  }
#endif
 
//  if (timer > 3000) {
//    timer = 0;
//    Serial.print("Processor Usage (%): "); Serial.println(AudioProcessorUsage());
// }

  delay(20); // Without some minimal delay here it will be difficult for the pots/switch changes to be detected.
}


// ////////////////////////////// servicing routines

void serviceKnobsAndButtons(void) {
  float potValue;

  //service the three pots
  for (int i=0; i<N_CHAN; i++) {
    if (controls.checkPotValue(pot_handle[i], potValue)) { //returns true if value has changed
      int new_val = chooseNoteNum(potValue,centerNotes[i]);
      if (new_val != notes[i].noteNum) {
        notes[i].noteNum = new_val;
        notes[i].startNote();  //play an example of the note
        Serial.println("New NoteNum[" + String(new_val) + "] = " + String(new_val)); 
      }
    }
  }

  // Check if SW1 has been toggled (pushed)
  if (controls.isSwitchToggled(button1_handle)) {
    int ret_val =  notes[0].startNote();
    Serial.print("Started note "); Serial.println(ret_val);
  }

  // Check if SW2 has been toggled (pushed)
  if (controls.isSwitchToggled(button2_handle)) {
    int ret_val = notes[1].startNote();
    Serial.print("Started note "); Serial.println(ret_val);
  }
}

void serviceLEDs(void) {
   //update LED stats
  if (notes[1].is_active) {
    //light both LEDs if notes[1] is active
    controls.setOutput(led1_handle, true); // Set the LED when NOT bypassed  
    controls.setOutput(led2_handle, true); // Set the LED when NOT bypassed  
  } else {
    //or, light LED1 if note[0] is active and LED2 if note[2] is active
    controls.setOutput(led1_handle, notes[0].is_active); // Set the LED when NOT bypassed  
    controls.setOutput(led2_handle, notes[2].is_active); // Set the LED when NOT bypassed  
  }
}

void serviceNotes(void) {
  static bool first_time = true;
  static unsigned int first_call_millis = 0;

  if (first_time) {
    first_call_millis = millis();
    first_time = false;
  } else {
    if ( (millis() - first_call_millis) > 2000 ) { //wait for a certain amount of time before...
      //enabling the notes to be active
      for (int Ichan=0; Ichan<N_CHAN;Ichan++) notes[Ichan].enabled=true;
    }
  }
    
  
  //check whether the notes should time out
  for (int Ichan = 0; Ichan < N_CHAN; Ichan++) {
    int ret_val = notes[Ichan].autoStopNote();
     if (ret_val > 0) {
      //Serial.println("Turned off Note " + String(Ichan));
     }
  }
}
