
#ifndef _AudioRMS_h
#define _AudioRMS_h

class AudioRMS : public AudioStream {
  public:
    AudioRMS(void) : AudioStream(1,inputQueueArray) { }

    void update(void) {
      //get the input audio data block
      audio_block_t *in_block = receiveReadOnly();
      if (!in_block) {
        //Serial.println("No in_block.");
        return;
      }

      //get the output data block
      audio_block_t *out_block = allocate();
      if (!out_block) { 
        //Serial.println("No out_block.");
        release(in_block); 
        return; 
      }

      //Serial.print("AudioRMS: data[0] = ");Serial.println(in_block->data[0]);
    
      //compute RMS
      int int_val; float float_val;
      float one_minus_coeff = 1.0f - filter_coeff;
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
        int_val = in_block->data[i];
        float_val = float(int_val) / 32767.0f;
        filt_wav2  = filter_coeff*(float_val*float_val) + one_minus_coeff*filt_wav2;
        out_block->data[i] = (int)(32767.0f*sqrtf(filt_wav2)+0.5);
      }
  
      //transmit the block and be done
      transmit(out_block);
      release(out_block);
      release(in_block);
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
    audio_block_t *inputQueueArray[1]; //memory pointer for the input to this module
    float filter_coeff = 1.0;
    float filt_wav2=0.0;
    
  
};

#endif
