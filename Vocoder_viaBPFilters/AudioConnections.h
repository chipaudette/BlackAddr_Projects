

// Define audio classes
AudioInputI2S              i2s_in;   //Audio in
AudioFilterBiquad          dcblock[2];  //remove any leftover DC bias
AudioFilterStateVariable   bp_voice[N_CHAN],bp2_voice[N_CHAN];     // for voice 

AudioRMS                   rms_voice[N_CHAN];          // for measuring the outout of the voice_bp
AudioFilterStateVariable   bp_synth[N_CHAN],bp2_synth[N_CHAN];  // for synth (or whatever) 
AudioEffectMultiply        mult_synth[N_CHAN];         // for changing the loudness of the synth

#define N_MIXER 4   //should be equal to ceil(N_CHAN/4)
AudioMixer4                mixer[N_MIXER]; //for mixing the processed synth
AudioMixer4                mixer_mixer;   //for mixinsg the mixers togethre
AudioAmplifier             final_gain;
AudioOutputI2S             i2s_out;  //audio output 

// Define audio connections
#define OUTPUT_LP 0  //for the state variable filter class
#define OUTPUT_BP 1 //for the state variable filter class
#define OUTPUT_HP 2 //for the state variable filter class 
AudioConnection* patchCables[200]; //make big enough to not worry about it
int makeConnections(void) {
  int ind=0;

  //connect inputs to DC blocking
  patchCables[ind++] = new AudioConnection(i2s_in, 0, dcblock[0], 0);
  patchCables[ind++] = new AudioConnection(i2s_in, 1, dcblock[1], 0);

  //each filter
  for (int i=0; i < N_CHAN; i++) {
    //voice
    patchCables[ind++] = new AudioConnection(dcblock[0],0,bp_voice[i],0);
    patchCables[ind++] = new AudioConnection(bp_voice[i],OUTPUT_BP,bp2_voice[i],0);
    patchCables[ind++] = new AudioConnection(bp2_voice[i],OUTPUT_BP,rms_voice[i],0);
    patchCables[ind++] = new AudioConnection(rms_voice[i],0,mult_synth[i],1);;

    //synth
    patchCables[ind++] = new AudioConnection(dcblock[1],0,bp_synth[i],0);
    patchCables[ind++] = new AudioConnection(bp_synth[i],OUTPUT_BP,bp2_synth[i],0);
    patchCables[ind++] = new AudioConnection(bp2_synth[i],OUTPUT_BP,mult_synth[i],0);

   //mixer
   int ind_mixer = (i / 4); //integer division means floor()
   int ind_input = i - (ind_mixer*4);
   //Serial.println("makeConnections: " + String(i) + ", mixer " + String(ind_mixer) + ", mix input " + String(ind_input));
   patchCables[ind++] = new AudioConnection(mult_synth[i],0,mixer[ind_mixer],ind_input);
   mixer[ind_mixer].gain(ind_input,1.0);
   
  }

  //connect to mixer of mixers
  if (N_MIXER > 4) Serial.println("makeConnections: *** WARNING ***: N_MIXER must be 4 or less!");
  for (int i=0; i < min(4,N_MIXER);i++) { //do NOT EXCEED 4!
    //Serial.println("makeConnections: connecting mixer " + String(i) + " to mixer_mixer");
    patchCables[ind++] = new AudioConnection(mixer[i],0,mixer_mixer,i); 
    mixer_mixer.gain(i,1.0);
  }

  //Serial.println("makeConnections: adjusting mixer_mixer gains...");
  //mixer_mixer.gain(0,0.25);
  //mixer_mixer.gain(1,0.5);
  //mixer_mixer.gain(2,1.0);
  //mixer_mixer.gain(3,1.0);

  //connect to output
  patchCables[ind++] = new AudioConnection(mixer_mixer,0, final_gain, 0);

  //connect to output
  patchCables[ind++] = new AudioConnection(final_gain,0, i2s_out, 0);
  patchCables[ind++] = new AudioConnection(final_gain,0, i2s_out, 1);

  return ind;
}


void setupSignalProcessing(void) {
  dcblock[0].setHighpass(0,40.0f,0.707);
  dcblock[1].setHighpass(0,40.0f,0.707);

  for (int i=0; i < N_CHAN; i++) {
    float octave_frac = 2.5; //1 is whole-octave, 2 is half-octave, 3 is third-octave, etc
    float bp_center_Hz = 125.*powf(2.0,float(i)/octave_frac); //every partial octave
    float filter_Q = 4.0;  //bandwidth = fc/Q...so 1-octave width requires resonance of 1.0, 1/2-octave = 2.0, etc
    Serial.println("Setting filter " + String(i) + " to " + String(bp_center_Hz) + "Hz with Q " + String(filter_Q)); 

    //configure the BP for the voice
    bp_voice[i].frequency(bp_center_Hz); 
    bp_voice[i].resonance(filter_Q); 
    bp_voice[i].octaveControl(0.0); 

    bp2_voice[i].frequency(bp_center_Hz); 
    bp2_voice[i].resonance(filter_Q); 
    bp2_voice[i].octaveControl(0.0); 

     //configure the RMS for the voice
     float rms_ave_sec = 0.005;  //make bigger to make slower
     rms_voice[i].set_ave_sec(rms_ave_sec);

    //configure the BP for the synth
    bp_synth[i].frequency(bp_center_Hz); 
    bp_synth[i].resonance(filter_Q);
    bp_synth[i].octaveControl(0.0);  

    bp2_synth[i].frequency(bp_center_Hz); 
    bp2_synth[i].resonance(filter_Q);
    bp2_synth[i].octaveControl(0.0); 

    //adjust gain based on frequency
    int ind_mixer = (i / 4); //integer division means floor()
    int ind_input = i - (ind_mixer*4);
    float gains[] = {0.25, 1.0};
    float freqs_Hz[] = {300.0, 1000.0};
    if (bp_center_Hz <= freqs_Hz[0]) {
      mixer[ind_mixer].gain(ind_input,gains[0]);
    } else if (bp_center_Hz >= freqs_Hz[1]) {
      mixer[ind_mixer].gain(ind_input,gains[1]);
    } else {
      float log_freq = logf(bp_center_Hz);
      float new_gain = ((log_freq - logf(freqs_Hz[0])) / (logf(freqs_Hz[1])-logf(freqs_Hz[0])))*(gains[1]-gains[0]) + gains[0];
      mixer[ind_mixer].gain(ind_input,new_gain);
    }
    
  }

  float final_gain_dB = 6.0;
  final_gain.gain(sqrtf(powf(10.0f, final_gain_dB/10.0f)));
}
