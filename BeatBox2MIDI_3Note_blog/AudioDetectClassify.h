
#ifndef _AudioDetectClassify_h
#define _AudioDetectClassify_h

class AudioDetectClassify : public AudioStream {
  public:
    AudioDetectClassify(void) : AudioStream(N_CHAN,inputQueueArray) {
      for (int i=0;i<N_CHAN;i++) {
        setThreshold_dBFS(i,-20.0f);
        is_above[i]=0;
      }
    }

    void update(void) {

      // /////////////////////// get input and output data blocks
      audio_block_t* in_block[N_CHAN];
      audio_block_t* out_block[N_CHAN];
      audio_block_t* out2_block[N_CHAN];
      bool did_allocation_fail = false;
      bool did_receive_fail = false;
      for (int i=0; i<N_CHAN;i++) {
        //get the input audio data block
        in_block[i] = receiveReadOnly(i);
        if (in_block[i] == NULL) did_receive_fail = true;

        //get the output audio blocks
        out_block[i] = allocate();
        if (out_block[i]==NULL) did_allocation_fail = true;

        //get the second set of output audio blocks
        out2_block[i] = allocate();
        if (out2_block[i]==NULL) did_allocation_fail = true;
      }
      if ((did_receive_fail) || (did_allocation_fail)) {
        //release data and return
        //if (did_receive_fail) Serial.println("AudioDetectClassify: failed to receive one or more channels");
        //if (did_allocation_fail) Serial.println("AudioDetectClassify: failed to allocate one or more audio blocks");
        for (int i=0; i<N_CHAN;i++) { release(in_block[i]); release(out_block[i]); release(out2_block[i]); }
        return;
      }

      //clear the output blocks
      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
          out_block[Ichan]->data[i] = 0;
          out2_block[Ichan]->data[i] = 0;
        }
      }

      // /////////////////////////// Do Audio Processing!
      float sig_pow_per_band[AUDIO_BLOCK_SAMPLES][N_CHAN]; //create some working memory
      
      // Loop over each channel and then over each sample, detect if impulse is present
      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        //apply thresholds
        for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
          int val = abs(in_block[Ichan]->data[i]);
          sig_pow_per_band[i][Ichan] = val*val; //use this later

          //to reduce flickering of detections right around the threshold, let's use one threshold 
          //for upward going signals and then, once the signal has crossed the threshold, a different
          //(lower) threshold for the signal falling back down.
          
          if (val > threshold[Ichan]) {
            is_above[Ichan] = true;
          } else {
            if (is_above[Ichan]==true) { //if we're already in the triggered state, apply a tougher criteria for downward
              if (val < down_threshold[Ichan])  is_above[Ichan] = false;
            }
          }
        }
      }

      // loop over each channel and normalize each filter band for loudness
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
        float sum_pow = 0.0;
        // get the total loudness
        for (int Ichan=0; Ichan < N_CHAN; Ichan++) sum_pow += sig_pow_per_band[i][Ichan]; //sum to get the total power
 
        //normalize by total loudness
        for (int Ichan=0; Ichan < N_CHAN; Ichan++) sig_pow_per_band[i][Ichan] /= sum_pow; //normalize by total power 

        //copy to second outputs for transmitting
        for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
          float val_dB = 10.0f*log10f(sig_pow_per_band[i][Ichan]);
          out2_block[Ichan]->data[i] = (int)(10.0f*val_dB + 0.5); //multiply by 10 and round (so we're sending to the 10th of a dB
        }
      }

      //now, compare to our classification criteria
      const int low=0, mid=1, high=2;
      const int OUT_VAL = 32767;
      const float kick_thresh_dB = -27.0+0.0; //tweak the value based on experiment
      const float hat_clap_divide_dB = -15.0;
      const float required_hat_thresh_dB = -10.0;
      const float required_clap_thresh_dB = -25.0;
      const float kick_thresh_pow = powf(10.0f, kick_thresh_dB/10.0f), hat_clap_divide_pow = powf(10.0f, hat_clap_divide_dB/10.0f);
      const float required_hat_thresh_pow = powf(10.0f, required_hat_thresh_dB/10.0f), required_clap_thresh_pow = powf(10.0f, required_clap_thresh_dB/10.0f);
      for (int i=0; i < AUDIO_BLOCK_SAMPLES; i++) {
        bool is_detected = false;
        for (int Ichan=0; Ichan < N_CHAN; Ichan++) if (is_above[Ichan]) is_detected = true; //are there any detections in the three bands?

        //do classification only if there is a detection
        //this simple 2D classifier is from the blog post 
        if (is_detected) {
          //look for kick
          if (sig_pow_per_band[i][high] <= kick_thresh_pow) { //if there are not much highs, it must be kick
            //not much highs...so it is a kick!
            out_block[low]->data[i] = OUT_VAL;
          } else {
            //there are highs.  Now look at mids.

            /* these are the original rules, as described in the blog post
            if (sig_pow_per_band[i][mid] <= hat_clap_divide_pow) { //if there are highs but not much mids, it must be hihat
              //not much mids...so it is a hihat!
              out_block[high]->data[i] = OUT_VAL;
            } else {
              //there are mids..so it is a clap!
              out_block[mid]->data[i] = OUT_VAL;
            }
            */

            //these are the revised rules
            if (sig_pow_per_band[i][mid] >= hat_clap_divide_pow) { //lots of mids?  must be clap!
              out_block[mid]->data[i] = OUT_VAL;
            } else { //not much mids
              if (sig_pow_per_band[i][high] >= required_hat_thresh_pow) { //lots and lots of highs...must be hats
                out_block[high]->data[i] = OUT_VAL;
              } else if (sig_pow_per_band[i][mid] >= required_clap_thresh_pow) { //here's a secondary way to trigger the clap...some mids, but not enough highs for a hat
                out_block[mid]->data[i] = OUT_VAL;
              }
            }
          }
        }
      }

      // /////////////////////////////////// Finished with the core signal processing

      // finish by transmitting the output blocks and releaseing the memory
      for (int Ichan=0; Ichan < N_CHAN; Ichan++) {
        transmit(out_block[Ichan],Ichan);
        transmit(out2_block[Ichan],Ichan+N_CHAN);
        release(out_block[Ichan]);
        release(out2_block[Ichan]);
        release(in_block[Ichan]);
      }
      
    } //end of update()

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
    bool is_above[N_CHAN];


};

#endif
