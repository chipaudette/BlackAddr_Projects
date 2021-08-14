#include <Audio.h>
#include "BALibrary.h" //for BlackAddr hardware: https://github.com/Blackaddr/BALibrary
#include "BAEffects.h"

using namespace BALibrary; // This prevents us having to put BALibrary:: in front of all the Blackaddr Audio components
BAAudioControlWM8731 codecControl;

AudioInputI2S           i2s_in;           //xy=365,94
AudioOutputUSB          usb_out;     
AudioOutputI2S          i2s_out;  

AudioConnection          patchCord1(i2s_in, 0, usb_out, 0);
AudioConnection          patchCord2(i2s_in, 1, usb_out, 1);
AudioConnection          patchCord11(i2s_in, 0, i2s_out, 0);
AudioConnection          patchCord12(i2s_in, 1, i2s_out, 1);


void setupAICconfiguration(void) {
  codecControl.setLeftInputGain(23); // 23 = 10111 = 0 dB of CODEC analog gain
  codecControl.setRightInputGain(23);
  codecControl.setLeftInMute(false); //make sure not muted
  codecControl.setRightInMute(false); //not muted
  codecControl.setHPFDisable(false);  // Start with the HPF enabled.
  delay(100);

  codecControl.recalibrateDcOffset();
}


void setup() {                
 Serial.begin(115200);  Serial1.begin(31250); 
  delay(1000);
  Serial.println("BeatBox2MIDI: Staring...");
  
  //TGA_PRO_MKII_REV1(); // Declare the version of the TGA Pro you are using.
  TGA_PRO_REVB(x);
  //TGA_PRO_REVA(x);

  delay(5); // wait a few ms to make sure the GTA Pro is fully powered up
  AudioMemory(100); // Provide an arbitrarily large number of audio buffers (48 blocks) for the effects

  // If the codec was already powered up (due to reboot) power it down first
  Serial.println("restarting codec...");
  codecControl.disable();  delay(100);
  codecControl.enable();  delay(100);

  //setup AIC configuration
  Serial.println("setupAICconfiguration()...");
  setupAICconfiguration();
  
  Serial.println("setup() complete...");
}

void loop() {

  delay(10);
}
