#pragma once
#include "audio_backend.h"
#include "lfqueue.h"

class AndroidAudio : public AudioBackend {
public:
    AndroidAudio(const char* appname, Lfq_u32* qnote, Lfq_u32* qcomm, Lfq_u8* qmidi);
    ~AndroidAudio() override;

    // Required by AudioBackend
    void start(void) override;          // no-op on Android; Oboe starts us
    int  relpri(void) const override;   // return 0 or something sensible

    // Called from Oboe callback
    int audio_callback(float* outInterleavedStereo, int nframes);

    // JNI entry: enqueue a 3-byte MIDI message
    bool push_midi(uint8_t status, uint8_t data1, uint8_t data2);

private:
    Lfq_u8* _qmidi;

    // temp planar buffers; proc_synth writes into _outbuf[]
    float* _tmp[2]{nullptr, nullptr};
    int    _tmp_cap = 0;

    void ensure_tmp(int nframes);

    void drain_midi();  // drain _qmidi and call MidiProcessor::process_midi_event
};

