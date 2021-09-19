
#ifndef _AudioThreshold_F32_h
#define _AudioThreshold_F32_h

#include <AudioStream_F32.h>

class AudioThreshold_F32 : public AudioStream_F32 {
  public:
    AudioThreshold_F32(void) : AudioStream_F32(1,inputQueueArray) {};

   void update(void) {
 
      //get the input audio data block
      audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32();
      if (!in_block) { return; }

      //get the output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) { release(in_block); return; } 

      processAudioBlock(in_block, out_block);
      
      //transmit the block and be done
      if (out_block != NULL) transmit(out_block);
      release(out_block); release(in_block);
    } //end update


    void processAudioBlock(audio_block_f32_t *in_block, audio_block_f32_t *out_block) {
      if ((in_block == NULL) || (out_block == NULL)) return;

      //make easier names for the arrays
      float *in = in_block->data;
      float *out = out_block->data;

      //step through the input data and look for crossings
      for (int i=0; i < in_block->length; i++) {
        if (fabs(in[i]) > threshold) {
          out[i] = 1.0f;
        } else {
          out[i] = 0.0f;
        }        
      }
      last_output = out[(in_block->length)-1];
    }
    
    float setThreshold_dB(float val_dB) {
      threshold = sqrtf(powf(10.0f,val_dB/10.0f));
      return getThreshold_dB();
    }
    float getThreshold_dB(void) { return 10.0*log10f(threshold*threshold); }
    float getValue(void) { return last_output; }

  private:
    audio_block_f32_t *inputQueueArray[1]; //memory pointer for the input to this module
    float threshold = 0.5;
    float last_output = 0.0;
};


#endif
