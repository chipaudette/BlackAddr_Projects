
#ifndef _AudioRMS_F32_h
#define _AudioRMS_F32_h

#include <AudioStream_F32.h>

class AudioRMS_F32 : public AudioStream_F32 {
  public:
    AudioRMS_F32(void) : AudioStream_F32(1,inputQueueArray) { }

    void update(void) {
      //get the input audio data block
      audio_block_f32_t *in_block = receiveReadOnly_f32();
      if (!in_block) {
        //Serial.println("No in_block.");
        return;
      }

      //get the output data block
      audio_block_f32_t *out_block = allocate_f32();
      if (!out_block) { 
        //Serial.println("No out_block.");
        release(in_block); 
        return; 
      }

      //Serial.print("AudioRMS: data[0] = ");Serial.println(in_block->data[0]);
    
      processAudioBlock(in_block,out_block);
  
      //transmit the block and be done
      transmit(out_block);
      release(out_block);
      release(in_block);
    }

    void processAudioBlock(audio_block_f32_t *in_block, audio_block_f32_t *out_block) {
      if ((in_block == NULL) || (out_block == NULL)) return;
      
      //compute RMS
      float float_val;
      float one_minus_coeff = 1.0f - filter_coeff;
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float_val = in_block->data[i];
        filt_wav2  = filter_coeff*(float_val*float_val) + one_minus_coeff*filt_wav2;
        out_block->data[i] = sqrtf(filt_wav2);
      }
    }
    
    float get_rms(void) { return sqrtf(filt_wav2); };
    float get_rms_dBFS(void) { return 10.0f*log10f(filt_wav2); }

    float set_ave_sec(float time_sec) { 
      float dt_sec = 1.0f / float(AUDIO_SAMPLE_RATE_EXACT);
      float time_samp = time_sec / dt_sec;
      filter_coeff = min(1.0f,max(0.0f,1./time_samp));
      return time_sec;
    }
    
  protected:
    audio_block_f32_t *inputQueueArray[1]; //memory pointer for the input to this module
    float filter_coeff = 1.0;
    float filt_wav2=0.0;
    
  
};

#endif
