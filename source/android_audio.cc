#include "android_audio.h"
#include "midi_processor.h"
#include <cstdlib>
#include <cstring>

AndroidAudio::AndroidAudio(const char* appname, Lfq_u32* qnote, Lfq_u32* qcomm, Lfq_u8* qmidi)
: AudioBackend(appname, qnote, qcomm)
, _qmidi(qmidi)
{
    // AudioBackend ctor already sets _nplay=2 in your build (per audio_backend.cc line 42)
    // but we can assert it if you like. We’ll keep it stereo for MVP.
    _nplay = 2;
}

AndroidAudio::~AndroidAudio() {
    std::free(_tmp[0]);
    std::free(_tmp[1]);
}

void AndroidAudio::start(void) {
    // On Android, Oboe drives callbacks; we don't create our own thread loop here.
    // Keep empty, but you still should call init_audio() from your native wrapper
    // once you know sample rate + buffer size.
}

int AndroidAudio::relpri(void) const {
    return 0; // Android priority is handled differently; keep simple for now.
}

void AndroidAudio::ensure_tmp(int nframes) {
    if (nframes <= _tmp_cap) return;
    int new_cap = 1;
    while (new_cap < nframes) new_cap <<= 1;
    _tmp_cap = new_cap;
    _tmp[0] = (float*)std::realloc(_tmp[0], sizeof(float) * _tmp_cap);
    _tmp[1] = (float*)std::realloc(_tmp[1], sizeof(float) * _tmp_cap);
}

bool AndroidAudio::push_midi(uint8_t status, uint8_t data1, uint8_t data2) {
    // Enqueue 3 bytes; must not block.
    return MidiProcessor::write_midi_queue(_qmidi, status, data1, data2);
}

void AndroidAudio::drain_midi() {
    if (!_qmidi) return;

    // read 3 bytes at a time: status,data1,data2
    while (_qmidi->read_avail() >= 3) {
        uint8_t status = _qmidi->read(0);
        uint8_t data1  = _qmidi->read(1);
        uint8_t data2  = _qmidi->read(2);
        _qmidi->read_commit(3);

        uint8_t channel = status & 0x0F;
        MidiProcessor::process_midi_event(status, data1, data2, channel,
                                          _midimap, this, _qnote, _qmidi);
    }
}

int AndroidAudio::audio_callback(float* outInterleavedStereo, int nframes) {
    // Mirror JackAudio::jack_callback sequencing (minus JACK buffer setup)
    proc_queue(_qnote);
    proc_queue(_qcomm);
    proc_keys1();
    proc_keys2();

    // MVP timing: process all queued MIDI at start of block
    drain_midi();
    proc_keys1(); // optional but matches JackAudio::proc_midi_during_synth() behavior

    // Set planar output buffers for proc_synth()
    ensure_tmp(nframes);
    _outbuf[0] = _tmp[0];
    _outbuf[1] = _tmp[1];

    proc_synth(nframes);
    proc_mesg();

    // Interleave into Oboe output buffer
    float* L = _tmp[0];
    float* R = _tmp[1];
    for (int i = 0; i < nframes; ++i) {
        outInterleavedStereo[2*i + 0] = L[i];
        outInterleavedStereo[2*i + 1] = R[i];
    }
    return 0;
}

