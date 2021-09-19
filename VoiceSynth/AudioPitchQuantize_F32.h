
#ifndef _AudioPitchQuantizer_F32_h
#define _AudioPitchQuantizer_F32_h

class AudioPitchQuantize_F32 : public AudioStream_F32 {
  public:
    AudioPitchQuantize_F32(void) : AudioStream_F32(1,inputQueueArray) { }


    void update(void) {
      //get the input audio data block
      audio_block_f32_t *in_block = AudioStream_F32::receiveReadOnly_f32(0);
      if (!in_block) { return; }

      //get the output data block
      audio_block_f32_t *out_block = AudioStream_F32::allocate_f32();
      if (!out_block) { release(in_block); return;  } 

      processAudioBlock(in_block, out_block);
      
      //transmit the block and be done
      if (out_block != NULL) transmit(out_block);
      release(out_block); release(in_block);
    } //end update

    void processAudioBlock(audio_block_f32_t *in_block, audio_block_f32_t *out_block) {
      if ((in_block == NULL) || (out_block == NULL)) return;

      for (int i=0; i<in_block->length;i++) {
        float in_Hz = in_block->data[i];
        float noteNum = freqToNoteNum_float(in_Hz);
        float quantNoteNum = (int)(noteNum + 0.5); //round to nearest integer
        float newNoteNum = (1.0f-quantize_amount)*noteNum + quantize_amount*quantNoteNum;
        out_block->data[i] = noteNumToFreq(newNoteNum);
      }
    }
    

    static int freqToNoteNum_int(float freq_Hz) { 
      return (int)(freqToNoteNum_float(freq_Hz)+0.5); //rounds to nearest integer noteNum
    }
    static float freqToNoteNum_float(float freq_Hz) { 
      //https://newt.phys.unsw.edu.au/jw/notes.html
      return 12.0*log2f(freq_Hz/440) + 69 -1;
    }
    float noteNumToFreq(float noteNum) {
      return 440.0f*powf(2.0,(noteNum+1-69)/12.0);
    }

    float setQuantizeAmount(float val) {  //0.0 is no quantize and 1.0 is fully quantized
      return quantize_amount = max(0.0,min(1.0,val));
    }
    float getQuanizeAmount(void) { return quantize_amount; }

  protected:
    audio_block_f32_t *inputQueueArray[1]; //memory pointer for the input to this module
    float quantize_amount = 1.0; //default to fully quantized
  
};

#endif
