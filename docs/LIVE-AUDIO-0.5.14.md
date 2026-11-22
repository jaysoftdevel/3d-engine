# Live spatial audio — JSD 0.5.14

`AudioMixer::update(Sound, V3 position, float gain, bool spatial = true)` updates
an active sound in place. It returns false for a stopped/finished handle, rejects
nonfinite position/gain, clamps negative gain to zero and uses the mixer's mutex.
It retains the voice cursor, loop state and shared clip. No clip copy, PCM rotation
or voice allocation occurs on a live update. Gain and stereo pan settle with a
25 ms exponential time constant, independent of mix block size and sample rate.

Camera-aware effects can calculate their own gain curve and call update each
frame. Existing native attenuation remains inverse-square with a 5 m reference.
Play/stop, WAV loading and the SDL device contract remain compatible. Rebuild
all consumers because the public mixer layout has changed.
