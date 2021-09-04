
#ifndef _TriggerMIDI_3Chan_h
#define _TriggerMIDI_h

#include "Note.h"

class AudioTriggerMIDI_3Chan : public AudioStream {
  public:
    AudioTriggerMIDI_3Chan(void) : AudioStream(2*N_CHAN,inputQueueArray) {};

    void update(void) {
      //bool any_prints = false;
      
      // /////////////////////// get input and output data blocks
      audio_block_t* in_block[2*N_CHAN];
      bool is_all_data = true;
      for (int i=0; i<N_CHAN;i++) {
        //get the input audio data block
        in_block[i] = receiveReadOnly(i);
        if (in_block[i] == NULL) {
          //Serial.println("AudioTriggerMIDI_3Chan: did not get chan " + String(i));
          is_all_data = false;
        }

        //debugging data
        in_block[N_CHAN+i] = receiveReadOnly(N_CHAN+i);
        if (in_block[N_CHAN+i] == NULL) {
          //Serial.println("AudioTriggerMIDI_3Chan: did not get chan " + String(N_CHAN+i));
          is_all_data = false;
        }
      }
      if (is_all_data == false) {
        //release data and return
        for (int i=0; i<2*N_CHAN;i++) { release(in_block[i]); }
        return;
      }
      
      //loop over the audio samples in this data block
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {

        //only test if we should trigger if we're not blocked out from the previous trigger
        if (block_new_trigger_until_samps == 0) {

          //now do the test to see if we should trigger
          bool is_above[N_CHAN];
          for (int Ichan=0;Ichan<N_CHAN;Ichan++) { is_above[Ichan] = (in_block[Ichan]->data[i] > trigger_thresh); }
          if (is_above[0] || is_above[1] || is_above[2]) {
            //if (!any_prints) { Serial.println("AudioTriggerMIDI_3Chan: above!"); any_prints=true;}

            //only trigger if we're going from below-threshold to above-threshold
            if (prev_state == false) {
              //yes! we want a note.  Which note?   We're only going to start one 
              if (is_above[0]) {
                if (note[0] != NULL) {
                  note[0]->startNote(); //if note has been allocated, start it!
                  printDebug(0,in_block[N_CHAN]->data[i],in_block[N_CHAN+1]->data[i],in_block[N_CHAN+2]->data[i]);
                }
              } else if (is_above[1]) {
                if (note[1] != NULL) {
                  note[1]->startNote(); //if note has been allocated, start it!
                  printDebug(1,in_block[N_CHAN]->data[i],in_block[N_CHAN+1]->data[i],in_block[N_CHAN+2]->data[i]);
                }
              } else {
                if (note[2] != NULL) { 
                  note[2]->startNote(); //if note has been allocated, start it!
                  printDebug(2,in_block[N_CHAN]->data[i],in_block[N_CHAN+1]->data[i],in_block[N_CHAN+2]->data[i]);
                }
              }

              //start the block-out timer
              block_new_trigger_until_samps = min_space_samps;
            }
            prev_state = true;
          } else {
            prev_state = false;
          }
        }
        if (block_new_trigger_until_samps > 0) block_new_trigger_until_samps--;
      }

      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        transmit(in_block[Ichan],Ichan);
        release(in_block[Ichan]);
        release(in_block[N_CHAN+Ichan]);
      }
    }

    String val10xToString(int val10x) { return String((float)val10x / 10.0f); }
    void printDebug(int i, int val1_10x, int val2_10x, int val3_10x) {
      Serial.println("Trigger: " + String(i) + ", " + val10xToString(val1_10x) + ", " + val10xToString(val2_10x) + ", " + val10xToString(val3_10x));
    }
      
    //Print* setSerial(Print *s) { serial_ptr = s; return getSerial(); }
    //Print* getSerial(void) { return serial_ptr; }

    void setNote(int chan, Note *n) { 
      chan = max(0,min(N_CHAN-1,chan));
      note[chan] = n;
    }
    
    unsigned long set_minSpaceSec(float sec) { return min_space_samps = (unsigned long)(0.5f+(sec*float(AUDIO_SAMPLE_RATE_EXACT))); }
    
  protected:
    audio_block_t *inputQueueArray[2*N_CHAN]; //memory pointer for the two inputs to this module
    //Print *serial_ptr = &Serial1;
    bool prev_state = false;
    //Note *note0, *note1;
    Note* note[2*N_CHAN];
    unsigned int block_new_trigger_until_samps = 0;
    unsigned int min_space_samps = (unsigned int)(((float)AUDIO_SAMPLE_RATE_EXACT) * 0.020f);
    int trigger_thresh = 32768/2;
    
  
};

#endif
