
#ifndef _Note_h
#define _Note_h

#ifndef MIDI_SEND_CHANNEL
#define MIDI_SEND_CHANNEL 1
#endif

////Need to reference the MIDI thing that is created by MIDI_CREATE_INSTANCE
//extern MIDI_NAMESPACE::MidiInterface<MIDI_NAMESPACE::SerialMIDI<HardwareSerial>> MIDI;

class Note {
  public:
    Note() {};
    bool enabled = false;
    int noteNum=40; //this ends up being the noteNum of the *next* note that will be issued
    int noteVel=127;
    bool is_active = false;
    int active_noteNum = 0; //this is the noteNum that was sent when starting the note
    unsigned long start_millis=0;
    unsigned long end_millis = 0;
    byte chan = MIDI_SEND_CHANNEL-1;
    unsigned long default_duration_millis = 125;
    unsigned long last_checked_millis; //use this to look for millis wrap-around

    int startNote(void) { //automatically stops the previous note
      if (!enabled) return 0;
      if (is_active) stopNote();
      return startNewNote();
    }

    int startNewNote(void) { //just starts the new note
      sendNoteOn(noteNum,noteVel,chan);
      is_active = true;
      active_noteNum = noteNum;
      start_millis = millis();
      last_checked_millis = start_millis;
      end_millis = start_millis + default_duration_millis;
      Serial.println("Note: starNewNote: " + String(noteNum));
      return noteNum;
    }

    int startLegatoNote(void) { //start the new note *then* stop the old note (ie, legato)
      //save previous note's data
      int prev_noteNum = active_noteNum;
      float was_active = is_active;

      //make new note
      startNewNote();

      //stop the old note
      if (was_active) sendNoteOff(prev_noteNum,0,chan);
      
      return noteNum; 
    }
    
    int stopStartNote(void) { //stop the previous note before starting the new note
      return startNote();
    }
    

    bool autoStopNote(void) {
      if (!is_active) return 0; //only try to turn off notes that are active
      if (millis() < last_checked_millis) { return stopNote(); } //millis wrapped around, so end the note!
      if (millis() > end_millis) { return stopNote(); }
      return 0;
    }

    int stopNote(void) {
      if (!is_active) return 0; //only turn off notes that are active
      Serial.println("Note: stopNote: " + String(active_noteNum));
      sendNoteOff(active_noteNum,noteVel,chan);
      is_active = false;
      return noteNum;
    }

    void sendNoteOn(int num, int vel, int chan) {
      if (!enabled) return;
      Serial1.write((byte)(0x90 + (byte)chan));Serial1.write(num);Serial1.write(vel);
    }
    void sendNoteOff(int num, int vel, int chan) {
      if (!enabled) return;
      Serial1.write((byte)(0x80 + (byte)chan));Serial1.write(num);Serial1.write(vel);
    }
};


#endif
