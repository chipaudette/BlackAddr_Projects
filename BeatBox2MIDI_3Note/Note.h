
#ifndef _Note_h
#define _Note_h

#ifndef MIDI_SEND_CHANNEL
#define MIDI_SEND_CHANNEL 10
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
    unsigned long default_duration_millis = 500;
    unsigned long last_checked_millis; //use this to look for millis wrap-around

    int startNote(void) {
      if (!enabled) return 0;
      if (is_active) stopNote();
      sendNoteOn(noteNum,noteVel,chan);
      is_active = true;
      active_noteNum = noteNum;
      start_millis = millis();
      last_checked_millis = start_millis;
      end_millis = start_millis + default_duration_millis;
      Serial.println("Note: start note " + String(noteNum));
      return noteNum;
    }

    bool autoStopNote(void) {
      if (!is_active) return 0; //only try to turn off notes that are active
      if (millis() < last_checked_millis) { return stopNote(); } //millis wrapped around, so end the note!
      if (millis() > end_millis) { return stopNote(); }
      return 0;
    }

    int stopNote(void) {
      if (!is_active) return 0; //only turn off notes that are active
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
