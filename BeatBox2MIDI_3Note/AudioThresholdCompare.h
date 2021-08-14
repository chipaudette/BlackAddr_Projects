
#ifndef _AudioThresholdCompare_h
#define _AudioThresholdCompare_h

class AudioThresholdCompare : public AudioStream {
  public:
    AudioThresholdCompare(void) : AudioStream(N_CHAN,inputQueueArray) {
      for (int i=0;i<N_CHAN;i++) {
        setThreshold_dBFS(i,-20.0f);
        is_above[i]=0;
      }
    }

    void update(void) {
      //bool any_prints = false;


      // /////////////////////// get input and output data blocks
      audio_block_t* in_block[N_CHAN];
      audio_block_t* out_block[N_CHAN];
      bool is_all_data = true;
      for (int i=0; i<N_CHAN;i++) {
        //get the input audio data block
        in_block[i] = receiveReadOnly(i);
        if (in_block[i] == NULL) is_all_data = false;

        //get the output audio blocks
        out_block[i] = allocate();
        if (out_block[i]==NULL) is_all_data = false;
      }
      if (is_all_data == false) {
        //release data and return
        for (int i=0; i<N_CHAN;i++) { release(in_block[i]); release(out_block[i]);  }
        return;
      }

      // /////////////////////////// Apply thresholds
      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        //apply thresholds
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
          int val = abs(in_block[Ichan]->data[i]);
          if (val > threshold[Ichan]) {
            is_above[Ichan] = true;
          } else {
            if (is_above[Ichan]==true) { //if we're already in the triggered state, apply a tougher criteria for downward
              if (val < down_threshold[Ichan]) {
                is_above[Ichan] = false;
              }
            }
          }
        }
      }

      //compare across channels
      const int low=0, mid=1, high=2;
      const int OUT_VAL = 32767;
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
        int val[N_CHAN];        
        float val_dB[3]; 
        for (int Ichan=0;Ichan<N_CHAN;Ichan++) {
          out_block[Ichan]->data[i] = 0; //clear output
          val[Ichan] = abs(in_block[Ichan]->data[i]);
          val_dB[Ichan] = 20.0f*log10f(val[Ichan]);
        }
        float ratio_low_high_dB = val_dB[low]-val_dB[high];
        float ratio_mid_high_dB = val_dB[mid]-val_dB[high];
        float ratio_mid_low_dB = val_dB[mid]-val_dB[low];

        
        if (is_above[low] && is_above[high]) {
          if (is_above[mid]) {
            //so, all three are above threshold
            if ((ratio_low_high_dB > rel_thresh_dB) && (ratio_mid_low_dB < -5)) {
              //low!
              out_block[low]->data[i]=OUT_VAL;
            } else {
              //high or mid
              if (ratio_mid_high_dB > -5) {
                //mid
                out_block[mid]->data[i]=OUT_VAL;
              } else {
                //high
                out_block[high]->data[i]=OUT_VAL;
              }
            } 
          } else { //no mid
            //just low and high
            if (ratio_low_high_dB > rel_thresh_dB) {
              //low!
              out_block[low]->data[i]=OUT_VAL;
            } else {
              //high!
              out_block[high]->data[i]=OUT_VAL;
            }
          }
        } else if (is_above[low] && is_above[mid]) { //no high
          if (ratio_mid_low_dB > -5) {
            //mid!
            out_block[mid]->data[i]=OUT_VAL;
          } else {
            //low!
            out_block[mid]->data[i]=OUT_VAL;
          }
        } else if (is_above[mid] && is_above[high]) { //now low
          if (ratio_mid_high_dB > -5) {
            //mid!
            out_block[mid]->data[i]=OUT_VAL;
          } else {
            //high!
            out_block[high]->data[i]=OUT_VAL;
          }
        } else if (is_above[low]) { //low only
          out_block[low]->data[i]=OUT_VAL;
        } else if (is_above[mid]) { //mid only
          out_block[mid]->data[i]=OUT_VAL;
        } else if (is_above[high]) { //high only
          out_block[high]->data[i]=OUT_VAL;
        }
         
      }

      //transmit the block and be done
      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        transmit(out_block[Ichan],Ichan);
        release(out_block[Ichan]);
        release(in_block[Ichan]);
      }
      
    }

    float setThreshold_dBFS(int chan, float thresh_dBFS) { 
      chan = max(0,min(N_CHAN-1,chan));
      int val = (int)(32767.0f*sqrtf(powf(10.0f,0.1f*thresh_dBFS))+0.5f); 
      threshold[chan]=val;
      down_threshold[chan]=val / 2;
      return getThreshold_dBFS(chan);
    }
    float getThreshold_dBFS(int chan) { 
      chan = max(0,min(N_CHAN-1,chan));
      return 20.0f*log10f((float)threshold[chan]/32767.0f);
    }
  protected:
    audio_block_t *inputQueueArray[N_CHAN]; //memory pointer for the two inputs to this module
    int threshold[N_CHAN];
    int down_threshold[N_CHAN];
    float rel_thresh_dB = 10.0;  //if both are triggered, you have to be above this to trigger chan0
    bool state_triggered=false;
    bool is_above[3];


};

#endif
