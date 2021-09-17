

// Define audio classes
AudioInputI2S_F32            i2s_in;   //Audio in
AudioFilterBiquad_F32        dcblock[2];
AudioFilterBiquad_F32        lp_filt1,lp_filt2;
AudioRMS_F32                 rms;
AudioThreshold_F32           voiceDetect;
AudioVoiceToFreq_F32         voiceToFreq;
AudioOutputI2S_F32           i2s_out;  //audio output 

// Define audio connections

AudioConnection_F32* patchCables[200]; //make big enough to not worry about it
int makeConnections(void) {
  int ind=0;

  //connect inputs to DC blocking
  patchCables[ind++] = new AudioConnection_F32(i2s_in, 0, dcblock[0], 0);
  patchCables[ind++] = new AudioConnection_F32(i2s_in, 1, dcblock[1], 0);

  //connect to loudness detection
  patchCables[ind++] = new AudioConnection_F32(dcblock[0],0,rms,0);
  patchCables[ind++] = new AudioConnection_F32(rms,0,voiceDetect,0);

  //connect to pitch sensing
  patchCables[ind++] = new AudioConnection_F32(dcblock[0],0,lp_filt1,0);
  patchCables[ind++] = new AudioConnection_F32(lp_filt1,0, lp_filt2,0);
  patchCables[ind++] = new AudioConnection_F32(lp_filt2,0, voiceToFreq,0);
  patchCables[ind++] = new AudioConnection_F32(voiceDetect,0, voiceToFreq,1);
  
  
  //connect raw input to output
  patchCables[ind++] = new AudioConnection_F32(voiceToFreq,0, i2s_out, 0);
  patchCables[ind++] = new AudioConnection_F32(i2s_in,1, i2s_out, 1);

  return ind;
}


void setupSignalProcessing(void) {
  //tell the objects what the sample rate is
  float fs_Hz = (float)AUDIO_SAMPLE_RATE;
  dcblock[0].setSampleRate_Hz(fs_Hz);
  dcblock[1].setSampleRate_Hz(fs_Hz);
  lp_filt1.setSampleRate_Hz(fs_Hz);
  lp_filt2.setSampleRate_Hz(fs_Hz);
  voiceToFreq.setSampleRate_Hz(fs_Hz);

  //set DC blocking
  float dc_block_freq_Hz = 50.0;
  dcblock[0].setHighpass(0,dc_block_freq_Hz,0.707);
  dcblock[1].setHighpass(0,dc_block_freq_Hz,0.707);

  //set voice detection
  rms.set_ave_sec(0.010);
  //voiceDetect.setThreshold_dB(160.0/4.0-90.0);
  voiceDetect.setThreshold_dB(-50.0);

  //set freuqency detection
  float lp_freq_Hz = 200.0;
  lp_filt1.setLowpass(0,lp_freq_Hz,0.707);
  lp_filt2.setLowpass(0,lp_freq_Hz,0.707);
  float max_freq_Hz = max(350.0, lp_freq_Hz);
  voiceToFreq.setMaxAllowedFreq_Hz(max_freq_Hz);

}
