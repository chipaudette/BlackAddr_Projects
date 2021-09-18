
#ifndef _AudioEffectVocoder_F32_h
#define _AudioEffectVocoder_F32_h

#define N_CHAN (8)

#include <AudioFilterBiquad_F32.h> //from Tympan Library
#include <AudioMathMultiply_F32.h> //from Tympan Library
#include "AudioRMS_F32.h"  //local

class AudioEffectVocoder_F32 : public AudioStream_F32 {
  public:
    AudioEffectVocoder_F32(void) : AudioStream_F32(2, inputQueueArray) {
      setupProcessing();
    };

    void update(void) {
      //get the input audio data block
      audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32(0);
      if (!in_block) {
        //Serial.println("No in_block.");
        return;
      }

      //get the input audio data block
      audio_block_f32_t *in_block2 = AudioStream_F32::receiveReadOnly_f32(1);
      if (!in_block2) {
        //Serial.println("No in_block.");
        release(in_block);
        return;
      }

      //get the output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) { 
        //Serial.println("No out_block.");
        release(in_block);
        release(in_block2);  
        return; 
      } 

      processAudioData(in_block, in_block2, out_block);
      
      //transmit the block and be done
      if (out_block != NULL) transmit(out_block);
      release(out_block);
      release(in_block);
      release(in_block2);
    } //end update

    void processAudioData(audio_block_f32_t *in_block, audio_block_f32_t *in_block2, audio_block_f32_t *out_block) {
      if ((in_block == NULL) || (in_block2 == NULL) || (out_block == NULL)) return;
      static int count=0;
      count++;

      //get some working audio memory
      audio_block_f32_t *foo_voice = allocate_f32();
      audio_block_f32_t *foo_synth = allocate_f32();
      if ((foo_voice == NULL) || (foo_synth==NULL)) {
        release(foo_voice); release(foo_synth);
        return;
      }

      // get a more useful name for the input audio
      audio_block_f32_t *in_synth = in_block, *in_voice = in_block2;

      //process each vocoder
      for (int Ichan=0; Ichan<N_CHAN; Ichan++) {
        //voice filtering
        bp_voice[Ichan].processAudioBlock(in_voice,foo_voice);
        bp2_voice[Ichan].processAudioBlock(foo_voice,foo_voice);

        //voic envelope follower
        rms_voice[Ichan].processAudioBlock(foo_voice,foo_voice);

        //synth filtering
        bp_synth[Ichan].processAudioBlock(in_synth,foo_synth);
        bp2_synth[Ichan].processAudioBlock(foo_synth,foo_synth);

        //vca to multiply the synth signal by the envleop of the voice signal
        for (int i=0; i<foo_synth->length;i++) foo_synth->data[i] *= foo_voice->data[i];

        //accumulate output
        if (Ichan == 0) {
          for (int i=0; i<foo_synth->length;i++) out_block->data[i] = (mixer_gain[Ichan]*foo_synth->data[i]);
        } else {
          for (int i=0; i<foo_synth->length;i++) out_block->data[i] += (mixer_gain[Ichan]*foo_synth->data[i]);
        }
      }

      //apply final gain
      for (int i=0; i<out_block->length;i++) out_block->data[i] *= final_gain;

      //set the id info for the outgoing block
      out_block->id = in_block->id;

      //release memory
      release(foo_voice); release(foo_synth);

      return;
    }
      


    void setupProcessing(void) {
      float octave_frac = 2.5; //1 is whole-octave, 2 is half-octave, 3 is third-octave, etc
      octave_frac = ((float)(N_CHAN-1)) / (logf(8000.0/125.0)/logf(2.0)); //gives 2.5 for N_CHAN = 16
      float filter_Q = 2.0;  //was 4.0.  bandwidth = fc/Q...so 1-octave width requires resonance of 1.0, 1/2-octave = 2.0, etc
      
      for (int i=0; i<N_CHAN;i++) {
        float bp_center_Hz = 125.*powf(2.0,float(i)/octave_frac); //every partial octave
        Serial.println("Setting filter " + String(i) + " to " + String(bp_center_Hz) + "Hz with Q " + String(filter_Q)); 
  
        //configure the BP for the voice and synth
        bp_voice[i].setBandpass(0,bp_center_Hz,filter_Q);
        bp2_voice[i].setBandpass(0,bp_center_Hz,filter_Q);
        bp_synth[i].setBandpass(0,bp_center_Hz,filter_Q);
        bp2_synth[i].setBandpass(0,bp_center_Hz,filter_Q);
  
        //configure the RMS for the voice
        float rms_ave_sec = 0.005;  //make bigger to make slower
        rms_voice[i].set_ave_sec(rms_ave_sec);

        //adjust gain based on frequency
        float gains[] = {0.25, 1.0};
        float freqs_Hz[] = {300.0, 1000.0};
        if (bp_center_Hz <= freqs_Hz[0]) {
          mixer_gain[i] = gains[0];
        } else if (bp_center_Hz >= freqs_Hz[1]) {
          mixer_gain[i] = gains[1];
        } else {
          float log_freq = logf(bp_center_Hz);
          float new_gain = ((log_freq - logf(freqs_Hz[0])) / (logf(freqs_Hz[1])-logf(freqs_Hz[0])))*(gains[1]-gains[0]) + gains[0];
          mixer_gain[i] = new_gain;
        }
      }

      float final_gain_dB = 10.0;
      final_gain = sqrtf(powf(10.0f,final_gain_dB/10.0));
    }

    float32_t mixer_gain[N_CHAN];
    float32_t final_gain = 1.0;
  protected:
    AudioFilterBiquad_F32 bp_synth[N_CHAN], bp2_synth[N_CHAN];
    AudioFilterBiquad_F32 bp_voice[N_CHAN], bp2_voice[N_CHAN];
    AudioRMS_F32 rms_voice[N_CHAN];

  private:
    audio_block_f32_t *inputQueueArray[2]; //memory pointer for the input to this module
    
};

 #endif
