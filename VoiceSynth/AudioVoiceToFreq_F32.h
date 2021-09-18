
#ifndef _AudioVoiceToFreq_F32_h
#define _AudioVoiceToFreq_F32_h

#include <stdlib.h> //for qsort()
#include <AudioStream_F32.h>


int compare_function(const void *a,const void *b) {
  int *x = (int *) a;
  int *y = (int *) b;
  return *x - *y;
}

class AudioVoiceToFreq_F32 : public AudioStream_F32 {
  public:
    AudioVoiceToFreq_F32(void) : AudioStream_F32(2,inputQueueArray) { }

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

      //offset the prev_crossing index by one block length
      prev_crossing_ind -= in_block->length;

      //make easier names for the arrays
      float *in = in_block->data;
      float *gate = in_block2->data;
      float *out = out_block->data;

      //step through the input data and look for crossings
      for (int i=0; i < in_block->length; i++) {
        
        //look for upward crossing of zero
        //if (i==0) { Serial.println(String(in[i],4) + String(", ") + String(prev_value,4)); }
        if (gate[i] > 0.5) { //gate should be 0.0 or 1.0
          if ((in[i] >= 0.0) && (prev_value < 0.0)) {
            //there is a new crossing!  compute the new frequency!
            recordCrossing(i,prev_crossing_ind);
            computeNewFreqency();
          }
        }

        //update state for next time
        prev_value = in[i];

        //set the output value to be the current frequency value
        out[i] = cur_freq_Hz;
      }
    } //end processAudioData()

    void recordCrossing(int new_crossing, int prev_crossing) {
      int new_period = new_crossing - prev_crossing;
      if (new_period >= min_allowed_period_samps) {
        //yes! record the crossing
        prev_period_samps[period_ind++] = new_period;
        while (period_ind > n_period) period_ind -= n_period;
        prev_crossing_ind = new_crossing;  //update this state value
      }
    }

    float computeNewFreqency(void) {
      int median_period_samps = getMedianPeriod_samps();
      float new_freq_Hz = ((float)sample_rate_Hz) / ((float)median_period_samps);
      if ((new_freq_Hz >= 50.0) && (new_freq_Hz <= max_allowed_freq_Hz)) cur_freq_Hz = new_freq_Hz;
      return cur_freq_Hz;
    }

    int getMedianPeriod_samps(void) {
     float foo[n_period]; 
     for (int i=0;i<n_period;i++) foo[i] = prev_period_samps[i];
     qsort(foo, sizeof(foo)/sizeof(*foo), sizeof(*foo), compare_function);
     int middle_ind = max(0,(n_period-1)/2);
     return foo[middle_ind];
    }

    float setMaxAllowedFreq_Hz(float freq_Hz) {
       max_allowed_freq_Hz = freq_Hz;
       min_allowed_period_samps = (int)(sample_rate_Hz / max_allowed_freq_Hz + 0.5f);
       return max_allowed_freq_Hz;
    }
    float setSampleRate_Hz(float fs_Hz) { 
      sample_rate_Hz = fs_Hz; 
      min_allowed_period_samps = (int)(sample_rate_Hz / max_allowed_freq_Hz + 0.5f);
      return getSampleRate_Hz();
    };
    float getSampleRate_Hz(void) { return sample_rate_Hz; }
    float getFrequency_Hz(void) { return cur_freq_Hz; }
  protected:
    audio_block_f32_t *inputQueueArray[2]; //memory pointer for the input to this module
    #define N_PERIOD 7
    int n_period = N_PERIOD;
    int period_ind = 0; //index for next write into prev_period_samps
    int prev_period_samps[N_PERIOD];
    int prev_crossing_ind = 0;
    float prev_value = -1.0;
    float sample_rate_Hz = (float)AUDIO_SAMPLE_RATE;
    float max_allowed_freq_Hz = 400.0f; //default...probably overwritten in main sketch
    int min_allowed_period_samps = AUDIO_SAMPLE_RATE/400;
    float cur_freq_Hz = 100.0;
};

#endif
