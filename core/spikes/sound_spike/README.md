# sound_spike

Throwaway experiment for physically based engine sound (plan.md iteration 3,
`docs/adr/0003-sound-approach.md`). Not part of simcore yet; the `acoustic/` code moves into
simcore in iteration 7.

## Run

```bash
cmake --build --preset release
./build/release/core/spikes/sound_spike/sound_spike             # interactive, plays sound
./build/release/core/spikes/sound_spike/sound_spike --export wav # 10 s comparison WAVs
```

| Key | Action |
|---|---|
| `w` / `s` | speed +/- 250 1/min (`W` / `S`: +/- 1000) |
| `d` / `a` | load +/- 10 % |
| `1` `2` `3` `4` | I4, I6, V8 crossplane, V8 flatplane |
| `r` | record the next 10 s to `recording_<layout>.wav` |
| `q` | quit |

Export scenario (each layout): 2 s idle (900 1/min, 15 % load), 5 s full-load run-up to
6500 1/min, 1 s hold, 2 s lift-off (throttle closed). Headphones show the V8 banks left/right.

## How it works (research.md §6)

```
crank angle (per sample) ──► EVO events ──► band-limited pulses (one per cylinder)
                                                │
              primaries ─► collector junction(s) ─► (Y-pipe) ─► tailpipe ─► open end
              (digital waveguides: two delay lines per pipe, losses, scattering junctions)
                                                │ radiated
                     muffler FIR (synthetic IR) ─► load-dependent low-pass ─► DC block ─► tanh
```

| File | Content |
|---|---|
| `src/dsp/` | delay line (fractional), FIR, one-pole / biquad / DC blocker, slew smoother, PRNG |
| `src/acoustic/layout.*` | I4, I6, V8 crossplane, V8 flatplane: firing order, pipes, groups |
| `src/acoustic/pulse_source.*` | crank integration, sub-sample EVO timing, pulse shape, jitter |
| `src/acoustic/exhaust_network.*` | waveguide network: valves, junctions, outlets |
| `src/acoustic/engine_sound.*` | one layout end to end, parameter smoothing |
| `src/app/sound_bank.*` | real-time front end: atomics, crossfade, recording buffer |
| `src/app/audio_device.*` | miniaudio adapter (only file that includes miniaudio) |
| `src/io/` | WAV writer, offline rendering |

## Real-time rules (research.md §6.8)

The audio callback (`SoundBank::render`) never allocates, locks or does I/O: all four layouts
and all buffers are created up front, parameters arrive through `std::atomic`, and the WAV
file of a recording is written by the main thread after the callback filled a preallocated
buffer.

## Tests

`ctest --preset debug` runs, among others, an **engine order analysis** (Goertzel algorithm)
that checks objectively what the ear should hear: I4 is dominated by order 2, I6 by order 3,
a flatplane V8 bank by order 2, while a crossplane bank has strong half orders (burble).
