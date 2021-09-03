

// Define audio classes
AudioInputI2S              i2s_in;   //Audio in
AudioFilterBiquad          dcblock;  //remove any leftover DC bias
AudioFilterStateVariable   lp[3];    //will be 3 filters in series 
AudioFilterStateVariable   bp[3];    //will be 3 filters in series
AudioFilterStateVariable   hp[3];    //will be 3 filters in series      
AudioRMS                   rms_lp, rms_hp, rms_bp; //custom class
AudioDetectClassify        detectClassify;   //custom class
AudioTriggerMIDI_3Chan     trigMIDI; //custom class
AudioOutputI2S             i2s_out;  //audio output 

// Define audio connections
#define OUTPUT_LP 0  //for the state variable filter class
#define OUTPUT_BP 1 //for the state variable filter class
#define OUTPUT_HP 2 //for the state variable filter class  
AudioConnection          patchCord1(i2s_in, 0, dcblock, 0); 
   
AudioConnection          patchCord10(dcblock, 0, lp[0], 0);       
AudioConnection          patchCord11(lp[0], OUTPUT_LP, lp[1], 0);
AudioConnection          patchCord12(lp[1], OUTPUT_LP, lp[2], 0);
AudioConnection          patchCord13(lp[2], OUTPUT_LP, rms_lp, 0);
AudioConnection          patchCord16(rms_lp, 0, detectClassify, 0);  //into first input of the thresholder
AudioConnection          patchCord18(detectClassify, 0, trigMIDI, 0); //into the first input of the MIDI trigger

AudioConnection          patchCord30(dcblock, 0, bp[0], 0);       
AudioConnection          patchCord31(bp[0], OUTPUT_BP, bp[1], 0);  //output1 is bandpass
AudioConnection          patchCord32(bp[1], OUTPUT_BP, bp[2], 0);  //output1 is bandpass
AudioConnection          patchCord33(bp[2], OUTPUT_BP, rms_bp, 0); //output1 is highpass
AudioConnection          patchCord36(rms_bp, 0, detectClassify, 1);    //into the second input of the thresholder
AudioConnection          patchCord38(detectClassify, 1, trigMIDI, 1);  //into the second input of the MIDI trigger

AudioConnection          patchCord20(dcblock, 0, hp[0], 0);       
AudioConnection          patchCord21(hp[0], OUTPUT_HP, hp[1], 0);  //output2 is highpass
AudioConnection          patchCord22(hp[1], OUTPUT_HP, hp[2], 0);  //output2 is highpass
AudioConnection          patchCord23(hp[2], OUTPUT_HP, rms_hp, 0); //output2 is highpass
AudioConnection          patchCord26(rms_hp, 0, detectClassify, 2);    //into the third input of the thresholder
AudioConnection          patchCord28(detectClassify, 2, trigMIDI, 2);  //into the third input of the MIDI trigger

AudioConnection          patchCord50(i2s_in, 0, i2s_out, 0);
AudioConnection          patchCord51(detectClassify, 0, i2s_out, 1); 


void setupSignalProcessing(void) {
  dcblock.setHighpass(0,40.0f,0.707);

  #if 1
    //from blog
    float lp_cutoff_Hz = 1000.0f;  //originally 400...raise this to better reflect the 1200 from the blog
    float bp_center_Hz = 2000.0f; //originally 2000
    float hp_cutoff_Hz = 3000.0f; //originally 3000  
  #else
    //empircally derived
    float lp_cutoff_Hz = 400.0f;  //originally 400...raise this to better reflect the 1200 from the blog
    float bp_center_Hz = 2000.0f; //originally 2000
    float hp_cutoff_Hz = 3000.0f; //originally 3000
  #endif

  for (int i=0; i < N_CHAN; i++) {
    lp[i].frequency(lp_cutoff_Hz); lp[i].resonance(0.707); lp[i].octaveControl(0.0);
    bp[i].frequency(bp_center_Hz); bp[i].resonance(0.707); bp[i].octaveControl(0.0);  //note that bandwidth = fc/Q, so for fc = 2000 and Q = 0.707, BW is 2828 Hz.  This is too fat?
    hp[i].frequency(hp_cutoff_Hz); hp[i].resonance(0.707); hp[i].octaveControl(0.0);
  }

  //setup RMS
  float ave_sec = 0.01f;
  rms_lp.set_ave_sec(ave_sec);
  rms_bp.set_ave_sec(ave_sec);
  rms_hp.set_ave_sec(ave_sec);

  //setup threshold
  float threshold_lp_dBFS = -40.0f; //was -35...a more negative number is more sensitive
  detectClassify.setThreshold_dBFS(0,threshold_lp_dBFS);
  trigMIDI.set_minSpaceSec(0.030);

  float threshold_bp_dBFS = -60.0f;
  detectClassify.setThreshold_dBFS(1,threshold_bp_dBFS);

  float threshold_hp_dBFS = -60.0f; //was -55
  detectClassify.setThreshold_dBFS(2,threshold_hp_dBFS);

  for (int Ichan=0; Ichan<N_CHAN; Ichan++) trigMIDI.setNote(Ichan,&(notes[Ichan]));
  
  Serial.println("Thresholds = " + String(detectClassify.getThreshold_dBFS(0),0) + ", " + String(detectClassify.getThreshold_dBFS(1),0) + ", " + String(detectClassify.getThreshold_dBFS(2),0));

}
