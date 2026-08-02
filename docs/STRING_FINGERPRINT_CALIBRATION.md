# String fingerprint calibration

This branch introduces the first non-neural calibration layer for identifying the physical guitar string behind a detected pitch.

## Reference setup

Use one repeatable signal chain:

- bridge pickup;
- guitar volume at 10;
- guitar tone at 10;
- clean DI signal;
- no compressor, distortion, amp simulation, modulation or reverb;
- stable tuning recorded in the profile.

Input amplitude is normalized by the fingerprint extractor. Small audio-interface gain changes should not require a new profile, but clipping must be avoided.

## Recording sequence

For each string, record three normal attacks at:

1. open string;
2. fret 5;
3. fret 12.

Recommended order:

```text
E2: 0, 5, 12
A2: 0, 5, 12
D3: 0, 5, 12
G3: 0, 5, 12
B3: 0, 5, 12
E4: 0, 5, 12
```

A future calibration assistant will average three takes per anchor. The current module accepts one already-selected audio segment per anchor.

## Stored fingerprint

Each anchor stores:

- 24 logarithmically spaced spectral-band ratios;
- attack-to-sustain ratio;
- normalized spectral centroid;
- high-frequency energy ratio;
- expected pitch position;
- attack energy ratio;
- sustain energy ratio;
- reference frequency and reference input level for diagnostics.

The feature vector is L2-normalized. Classification therefore depends mostly on spectral shape and temporal envelope, not absolute input gain.

## Classification constraints

The classifier first rejects strings that cannot physically produce the detected frequency in a 24-fret range. It then compares the observation with the nearest calibrated anchor using cosine similarity.

Output:

```cpp
struct StringMatch {
    GuitarString stringId;
    float confidence;
    float score;
};
```

This output is intended to condition the existing pitch detector, not replace it. A MIDI note should only be accepted when:

- the pitch detector is stable;
- the string score is plausible;
- the note is physically reachable on that string;
- the transition is compatible with the previous state of that string.

## Next implementation stages

1. Add a calibration state machine and LV2 controls.
2. Capture attack and sustain windows without allocation in the audio callback.
3. Save profiles through the LV2 state extension.
4. Feed string likelihoods into harmonic rejection.
5. Add six independent string states for MIDI/MPE output.
6. Collect labelled DI recordings for a causal neural embedding model.

NeuralNote remains a useful reference for RTNeural integration and note/onset/contour heads, but its Basic Pitch model is non-causal and is not used directly in this real-time path.
