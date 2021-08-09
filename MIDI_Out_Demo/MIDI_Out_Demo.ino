

//include libraries
#include <Audio.h>     //installed along with Teensyduino
#include "BALibrary.h" //for BlackAddr hardware: https://github.com/Blackaddr/BALibrary
//#include <MIDI.h>      //installed along with Teensyduino?


using namespace BALibrary; // This prevents us having to put BALibrary:: in front of all the Blackaddr Audio components
BAAudioControlWM8731 codecControl;
AudioInputI2S        i2sIn;
AudioOutputI2S       i2sOut;

//////////////////////////////////////////
// SETUP PHYSICAL CONTROLS
// - POT1 (left) will control the amount of delay
// - POT2 (right) will control the amount of feedback
// - POT3 (centre) will control the wet/dry mix.
// - SW1  (left) will be used as a bypass control
// - LED1 (left) will be illuminated when the effect is ON (not bypass)
// - SW2  (right) will be used to cycle through the three built in analog filter styles available.
// - LED2 (right) will illuminate when pressing SW2.
//////////////////////////////////////////
// To get the calibration values for your particular board, first run the
// BAExpansionCalibrate.ino example and 
constexpr int  potCalibMin = 1;
constexpr int  potCalibMax = 1018;
constexpr bool potSwapDirection = true;

// Create a control object using the number of switches, pots, encoders and outputs on the
// Blackaddr Audio Expansion Board.
BAPhysicalControls controls(BA_EXPAND_NUM_SW, BA_EXPAND_NUM_POT, BA_EXPAND_NUM_ENC, BA_EXPAND_NUM_LED);

// BAPhysicalControls returns a handle when you register a new control. We'll uses these handles when working with the controls.
int button1_handle, button2_handle, notenum1_handle, notenum2_handle, velocity_handle, led1_handle, led2_handle; // Handles for the various controls

// ///////////////////////////////////////// MIDI STUFF
//MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, MIDI);

// ///////////////////////////////////////// NOTE STUFF
#define MIDI_SEND_CHANNEL 10
#include "Note.h"
Note notes[2];

// Audio Connections: name(channel)
// - Setup a mono signal chain, send the mono signal to both output channels in case you are using headphone
// i2sIn(0) --> i2sOut(0)
// i2sIn(1) --> i2sOut(1)
AudioConnection      patch0(i2sIn, 0, i2sOut, 0);  // connect the cab filter to the output.
AudioConnection      patch1(i2sIn, 0, i2sOut, 1); // connect the cab filter to the output.

void setupKnobsAndButtons(void) {
  // Setup the controls. The return value is the handle to use when checking for control changes, etc.
  // pushbuttons
  button1_handle = controls.addSwitch(BA_EXPAND_SW1_PIN); // will be used for bypass control
  button2_handle = controls.addSwitch(BA_EXPAND_SW2_PIN); // will be used for stepping through filters
  
  // pots
  notenum1_handle = controls.addPot(BA_EXPAND_POT1_PIN, potCalibMin, potCalibMax, potSwapDirection); // control the amount of delay
  notenum2_handle = controls.addPot(BA_EXPAND_POT2_PIN, potCalibMin, potCalibMax, potSwapDirection); 
  velocity_handle = controls.addPot(BA_EXPAND_POT3_PIN, potCalibMin, potCalibMax, potSwapDirection); 
  
  // leds
  led1_handle = controls.addOutput(BA_EXPAND_LED1_PIN);
  led2_handle = controls.addOutput(BA_EXPAND_LED2_PIN); // will illuminate when pressing SW2
}

int chooseNoteNum(float pot_val) { //pot_val is 0.0 to 1.0
  return (int)(16.0*pot_val + 0.5)+36;
}

//Create setup() and loop() functions, per Arduino standard
elapsedMillis timer;
void setup() {
  Serial.begin(115200);  Serial1.begin(31250); 
  delay(1000);
  Serial.println("MIDI_Out_Demo: Staring...");
  
  //TGA_PRO_MKII_REV1(); // Declare the version of the TGA Pro you are using.
  TGA_PRO_REVB(x);
  //TGA_PRO_REVA(x);

  delay(5); // wait a few ms to make sure the GTA Pro is fully powered up
  AudioMemory(48); // Provide an arbitrarily large number of audio buffers (48 blocks) for the effects (delays use a lot more than others)

  //setup the knobs and buttons
  setupKnobsAndButtons();

  // If the codec was already powered up (due to reboot) power it down first
  codecControl.disable();  delay(100);
  codecControl.enable();  delay(100);

  //initialize MIDI
//  MIDI.begin(MIDI_CHANNEL_OMNI);  // Listen to all incoming messages
//  delay(200);

}



void loop() {  


  serviceKnobsAndButtons();
  serviceLEDs();
  serviceNotes();

 
//  if (timer > 3000) {
//    timer = 0;
//    Serial.print("Processor Usage (%): "); Serial.println(AudioProcessorUsage());
// }

  delay(20); // Without some minimal delay here it will be difficult for the pots/switch changes to be detected.
}


// ////////////////////////////// servicing routines

void serviceKnobsAndButtons(void) {
  float potValue;

  // POT1
  if (controls.checkPotValue(notenum1_handle, potValue)) {  //has the POT value changed?
    int new_val = notes[0].noteNum = chooseNoteNum(potValue);
    Serial.println(String("New NoteNum1 setting: ") + new_val);   
  }

  // POT2
  if (controls.checkPotValue(notenum2_handle, potValue)) {  //has the POT value changed?
    int new_val = notes[1].noteNum = chooseNoteNum(potValue);
    Serial.println(String("New NoteNum2 setting: ") + new_val);
  }

  // POT3
  if (controls.checkPotValue(velocity_handle, potValue)) { //has the POT value changed?
    int new_val = (int)(127.0*potValue + 0.5);
    Serial.println(String("New Velocity setting: ") + new_val);
    notes[0].noteVel = new_val;
    notes[1].noteVel = new_val;
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
  controls.setOutput(led1_handle, notes[0].is_active); // Set the LED when NOT bypassed  
  controls.setOutput(led2_handle, notes[1].is_active); // Set the LED when NOT bypassed  
}

void serviceNotes(void) {
  static bool first_time = true;
  static unsigned int first_call_millis = 0;

  if (first_time) {
    first_call_millis = millis();
    first_time = false;
  } else {
    if ( (millis() - first_call_millis) > 3000 ) { //wait for a certain amount of time before...
      //enabling the notes to be active
      notes[0].enabled=true;
      notes[1].enabled=true;
    }
  }
    
  
  //check whether the notes should time out
  if (notes[0].autoStopNote() > 0) Serial.println("Turned off Note1.");
  if (notes[1].autoStopNote() > 0) Serial.println("Turned off Note2.");
 
}
