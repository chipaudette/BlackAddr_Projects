
#ifndef _AudioEffectVocoder_F32_h
#define _AudioEffectVocoder_F32_h

#define MAX_N_VOCOD_CHAN (16)

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
      if (!in_block) { return; }

      //get the input audio data block
      audio_block_f32_t *in_block2 = AudioStream_F32::receiveReadOnly_f32(1);
      if (!in_block2) { release(in_block); return; }

      //get the output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) { release(in_block); release(in_block2);  return; } 

      processAudioBlock(in_block, in_block2, out_block);
      
      //transmit the block and be done
      if (out_block != NULL) transmit(out_block);
      release(out_block); release(in_block); release(in_block2);
    } //end update

    void processAudioBlock(audio_block_f32_t *in_block, audio_block_f32_t *in_block2, audio_block_f32_t *out_block) {
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
      for (int Ichan=0; Ichan<n_chan; Ichan++) {
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
      


    int setupProcessing(int _n_chan = MAX_N_VOCOD_CHAN) {
      mute_output = true;
      int n_chan_new = max(3.0,min(MAX_N_VOCOD_CHAN,_n_chan));
      //if (n_chan_new == n_chan) return n_chan;
      if (n_chan_new < n_chan) n_chan = n_chan_new;
      
      float start_freq_Hz = 62.5, end_freq_Hz = 12000.0;
      if (n_chan_new < 9) start_freq_Hz = 125.0; //with fewer channels start higher
      if (n_chan_new < 4) end_freq_Hz = 8000.0f;
      if (n_chan_new < 3) {start_freq_Hz = 250.0; end_freq_Hz = 3000.0; }
      float octave_frac = ((float)(n_chan_new-1)) / (logf(end_freq_Hz/start_freq_Hz)/logf(2.0)); //gives 2.5 for n_chan = 16
      float min_Q= 1.0, max_Q = 3.5;
      float filter_Q = max(min_Q,min(max_Q,2 * octave_frac));
      //float filter_Q = 4.0;  //was 4.0.  bandwidth = fc/Q...so 1-octave width requires resonance of 1.0, 1/2-octave = 2.0, etc


      Serial.println("AudioEffectVocoder: octave_frac = " + String(octave_frac) + ", Q = " + String(filter_Q));
      
      for (int i=0; i<n_chan_new;i++) {

        float bp_center_Hz = start_freq_Hz*powf(2.0,float(i)/octave_frac); //every partial octave
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
        //float gains[] = {0.25 , 1.0};
        float gains[] = {0.4 , 1.0};
        gains[0] = max(gains[0],min(1.0,gains[0] * n_chan / 8)); //make gains[0] closer to 1.0 with fewer channels
        float freqs_Hz[] = {300.0, 1000.0};
        if (bp_center_Hz <= 100) {
          mixer_gain[i] = gains[0]/2;
        } else if (bp_center_Hz <= freqs_Hz[0]) {
          mixer_gain[i] = gains[0];
        } else if (bp_center_Hz >= freqs_Hz[1]) {
          mixer_gain[i] = gains[1];
        } else {
          float log_freq = logf(bp_center_Hz);
          float new_gain = ((log_freq - logf(freqs_Hz[0])) / (logf(freqs_Hz[1])-logf(freqs_Hz[0])))*(gains[1]-gains[0]) + gains[0];
          mixer_gain[i] = new_gain;
        }
      }

      //float final_gain_dB = 10.0;
      float final_gain_dB = 10.0-3.0;
      final_gain = sqrtf(powf(10.0f,final_gain_dB/10.0));


      if (n_chan_new > n_chan) n_chan = n_chan_new; //when increasing, what until everything is defined
      mute_output = false;
      return n_chan;
    }

    float32_t mixer_gain[MAX_N_VOCOD_CHAN];
    float32_t final_gain = 1.0;
  protected:
    int n_chan = MAX_N_VOCOD_CHAN;
    AudioFilterBiquad_F32 bp_synth[MAX_N_VOCOD_CHAN], bp2_synth[MAX_N_VOCOD_CHAN];
    AudioFilterBiquad_F32 bp_voice[MAX_N_VOCOD_CHAN], bp2_voice[MAX_N_VOCOD_CHAN];
    AudioRMS_F32 rms_voice[MAX_N_VOCOD_CHAN];
    bool mute_output = false;
    

  private:
    audio_block_f32_t *inputQueueArray[2]; //memory pointer for the input to this module
    
};

 #endif
