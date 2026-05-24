TONE GENERATOR
==============

A lightweight Windows tone generator that produces pure sine tones from
first principles. No samples, no synth libraries -- every sound you hear
is computed mathematically in real time. Built in C with the classic
Win32 look.


GETTING STARTED
---------------

1. Run ToneGen.exe (no installation required).
2. Pick a preset from the dropdown, or type a frequency into the Custom
   field and click outside it to confirm.
3. Click Play. You will hear a pure sine tone.
4. Drag the Volume slider to adjust loudness.
5. Click Stop to fade out cleanly.

Note: after typing a custom frequency, you must click outside the edit
field (or tab away) before clicking Play, so the new value is committed.


MODES
-----

The program has two modes, selectable via the radio buttons:

  Single tone
    Both ears receive the same frequency. Good for meditation drones,
    tuning instruments, or testing speakers.

  Binaural
    Each ear receives a slightly different frequency. Your brain
    perceives a rhythmic "wobble" at the difference between the two.
    This perceived wobble is the binaural beat. Headphones are required
    for binaural beats to work -- speakers mix the channels and cancel
    the effect.


HOW BINAURAL BEATS WORK
-----------------------

A binaural beat is not a real sound in the air. It is created by your
brain when it receives two close but different frequencies, one in each
ear. The brain perceives a pulsing tone at the difference frequency.

Example: left ear = 200 Hz, right ear = 207.83 Hz.
Your brain hears a steady tone around 200 Hz with a 7.83 Hz wobble
layered on top. That 7.83 Hz wobble is the binaural beat. You cannot
hear 7.83 Hz directly (it is far below the ~20 Hz threshold of human
hearing), but through the binaural effect your brain entrains to it.

The formula:

    binaural beat frequency = |right ear - left ear|

So to target a specific binaural beat:

    right ear = left ear + desired beat

The "left ear" frequency (the carrier/base) can be anything comfortable
to listen to. Most people use 100-400 Hz as a carrier. The carrier is
what you consciously hear; the beat is what your brain entrains to.


USING BASE + BEAT (simple method)
---------------------------------

1. Switch to Binaural mode.
2. Type your carrier into the Base field (e.g. 200).
3. Type your desired binaural beat into the Beat field (e.g. 7.83).
4. The program automatically calculates:
     Left  = Base         = 200.000 Hz
     Right = Base + Beat  = 207.830 Hz
5. Put on headphones and click Play.

Examples:

  To get a 7.83 Hz Schumann beat:
    Base = 200, Beat = 7.83
    -> Left 200 Hz, Right 207.83 Hz

  To get a 10 Hz alpha beat:
    Base = 150, Beat = 10
    -> Left 150 Hz, Right 160 Hz

  To get a 40 Hz gamma beat:
    Base = 300, Beat = 40
    -> Left 300 Hz, Right 340 Hz


USING L/R DIRECTLY (advanced method)
-------------------------------------

1. Switch to Binaural mode.
2. Tick "Advanced (set L/R directly)".
3. Type the exact frequency for each ear.

This gives you full control. The binaural beat is always:

    beat = |R - L|

Examples:

  Schumann 7.83 Hz beat:
    L = 200, R = 207.83

  Theta 6 Hz beat:
    L = 250, R = 256

  Pure unison (no beat, same tone both ears):
    L = 432, R = 432

  Asymmetric carrier (different base tone per ear):
    L = 100, R = 540
    Beat = 440 Hz (this is audible as a distinct pitch difference,
    not a subtle wobble -- binaural entrainment works best when L
    and R are close together, typically within 30 Hz of each other)

Tip: for effective brainwave entrainment, keep the beat frequency
below ~40 Hz and keep L and R within about 30 Hz of each other. Wider
gaps produce an audible pitch difference rather than a perceived wobble.


SUB-AUDIBLE FREQUENCIES
-----------------------

Human hearing ranges from roughly 20 Hz to 20,000 Hz. Many of the
frequencies below (especially the brainwave entrainment ones) are far
below 20 Hz. You cannot hear them as a direct tone through speakers.

Use them as binaural beats instead: set the frequency as the Beat value,
pick a comfortable Base, and wear headphones. The status bar will warn
you if you try to play a sub-audible frequency in Single mode.


BUILDING FROM SOURCE
--------------------

Requirements: MinGW-w64 (gcc) and GNU make.

    make            Build ToneGen.exe
    make test       Run the unit tests
    make clean      Remove built files


FREQUENCY REFERENCE
-------------------

Below are frequencies commonly discussed in healing, meditation,
brainwave entrainment, and new age contexts. They are ranked roughly
from most widely cited to least. Frequencies marked with * are below
20 Hz and should be used as binaural beats, not direct tones.

 #   Hz        Name / Association
---  --------  -------------------------------------------------------
 1   432.000   Verdi tuning / "natural" A -- claimed to resonate with
               nature and water; popular alternative to standard 440 Hz

 2   528.000   Solfeggio MI / "Love frequency" / "DNA repair" -- the
               most cited solfeggio frequency; also called the
               "miracle tone"

 3     7.830*  Schumann resonance -- the fundamental electromagnetic
               resonance of the Earth's cavity; associated with
               grounding, calm, and connection to nature

 4   396.000   Solfeggio UT -- liberating guilt and fear

 5   417.000   Solfeggio RE -- facilitating change, undoing situations

 6   639.000   Solfeggio FA -- connecting and relationships

 7   741.000   Solfeggio SOL -- awakening intuition, expression

 8   852.000   Solfeggio LA -- returning to spiritual order

 9   963.000   Solfeggio -- pure miracle tone / pineal gland activation
               / "frequency of God"

10   174.000   Solfeggio (extended) -- natural anaesthetic, foundation,
               pain reduction

11   285.000   Solfeggio (extended) -- healing tissue, cellular repair

12    40.000*  Gamma brainwaves -- associated with insight, peak focus,
               higher cognitive function; research links 40 Hz to
               clearing amyloid plaques (Alzheimer's research)

13   440.000   Standard concert pitch A4 -- universal tuning reference

14   256.000   Scientific C / Verdi C -- C4 in scientific tuning
               (A=432); sometimes called the "healing C"

15    10.000*  Alpha brainwaves -- relaxation, calm alertness, light
               meditation; the classic "alpha state"

16     4.000*  Theta brainwaves -- deep meditation, dreaming, intuition,
               subconscious access

17     1.500*  Delta brainwaves -- deep dreamless sleep, healing,
               regeneration

18    14.000*  SMR (sensorimotor rhythm) -- relaxed focus, calm body
               with alert mind; used in neurofeedback

19    20.000   Low beta / edge of audibility -- mental alertness,
               active thinking

20   111.000   "Holy frequency" -- measured resonance in ancient temples
               (Hypogeum of Malta); said to stimulate the prefrontal
               cortex

21   136.100   OM frequency -- the frequency of Earth's year (Cosmic
               Octave calculation by Hans Cousto); used in Indian
               classical music as the reference SA

22   126.220   Sun frequency -- Cousto's octave calculation of Earth's
               orbital period around the Sun

23   194.180   Earth day frequency -- Cousto's calculation for one
               Earth rotation (24 hours) octaved into audible range

24   210.420   Moon frequency -- synodic month octaved up; associated
               with the sacral chakra

25   221.230   Venus frequency -- Venus orbital period octaved into
               audible range

26   144.720   Mars frequency -- associated with strength, energy,
               willpower

27   172.060   Platonic year -- the precession of the equinoxes
               (~25,772 years) octaved up; associated with higher
               spiritual connection

28   473.000   Solfeggio (extended set, sometimes cited) -- bridging
               frequency between heart and throat

29     3.500*  Deep theta / Schumann 2nd harmonic region -- profound
               meditation, trance states

30    33.000*  Christ consciousness frequency -- cited in some new age
               traditions as a resonance of spiritual awakening

31     0.500*  Epsilon brainwaves -- extremely deep states, suspended
               animation, extraordinary meditation

32   288.000   Double 144 / numerological frequency -- associated with
               abundance and light body activation

33   174.000   (see #10, also cited standalone as the "foundation
               frequency" for grounding)

34   360.000   "Balance" frequency -- 360 degrees of the circle;
               associated with wholeness and joy

35   480.000   Third harmonic of 160 / sometimes cited as the
               "liberation frequency"

36   720.000   Sometimes cited as a higher-octave healing frequency

37     7.000*  Theta-alpha border -- creativity, visualization, "the
               twilight zone" between waking and sleep

38   108.000   Sacred number frequency -- 108 is sacred in Hinduism,
               Buddhism, and Jainism; the frequency is used in sound
               healing

39   128.000   Two octaves below 512 Hz (medical tuning fork C);
               used for bone conduction healing

40   512.000   Medical tuning fork C -- used in clinical diagnostics;
               also cited in sound therapy

41     2.500*  Deep delta -- cellular regeneration, growth hormone
               release, immune system support

42    12.000*  Alpha-beta border -- centered awareness, cognitive
               bridge; used in learning and focus protocols

43    15.000*  Beta brainwaves -- active concentration, analytical
               thinking, problem-solving focus

44     6.300*  Theta (mid-range) -- creative visualization, emotional
               processing, self-healing

45   187.610   Mercury frequency -- Cousto orbital calculation; mental
               clarity, communication

46   295.700   Saturn frequency -- discipline, structure, boundaries

47   207.360   Uranus frequency -- spontaneity, revolution, awakening

48   211.440   Neptune frequency -- intuition, dreams, the unconscious

49   140.250   Pluto frequency -- transformation, regeneration, hidden
               depths

50    27.500   Sub-bass A0 -- the lowest A on a standard piano; at the
               edge of hearing, felt more than heard; grounding

51     0.100*  Hyper-delta / lambda -- sometimes cited as a carrier for
               "whole-brain synchronisation" at the lowest extreme

52    50.000   Gamma (low) / also mains hum in Europe -- some
               practitioners cite 50 Hz as a vitality frequency

53   384.000   Two octaves of 96 / sometimes called the "truth
               frequency" in certain healing lineages

54   586.000   Sometimes cited as a higher expression of Solfeggio RE
               (417 + 169)

55   888.000   "Angel number" frequency -- popular in numerology-based
               sound healing; associated with abundance


40 HZ BRAIN CLEANER
-------------------

The "40Hz Brain Cleaner" button at the bottom of the program is a
dedicated mode that replicates the auditory stimulation protocol used
in MIT's GENUS (Gamma ENtrainment Using Sensory stimuli) research.

When you click Start, the program plays a precise sequence of 1 ms tone
pips at a 10 kHz carrier frequency, repeating at exactly 40 Hz (once
every 25 ms). This matches the protocol described in:

  Martorell AJ, Paulson AL, Suk HJ, et al. (2019). "Multi-sensory
  Gamma Stimulation Ameliorates Alzheimer's-Associated Pathology and
  Improves Cognition." Cell, 177(2), 256-271.
  https://doi.org/10.1016/j.cell.2019.02.014

All other controls are disabled while the Brain Cleaner is running.
Only the volume slider remains active. Click Stop to return to normal
operation.


WHAT THE RESEARCH SAYS
----------------------

The work originates from Li-Huei Tsai's lab at MIT's Picower Institute
for Learning and Memory. Key findings:

  1. Iaccarino HF, Singer AC, Martorell AJ, et al. (2016). "Gamma
     frequency entrainment attenuates amyloid load and modifies
     microglia." Nature, 540(7632), 230-235.
     https://doi.org/10.1038/nature20587

     The original breakthrough. Exposing mice to 40 Hz light flicker
     reduced amyloid-beta plaques (a hallmark of Alzheimer's disease)
     in the visual cortex by roughly 50%. The stimulation triggered
     microglia (the brain's immune cells) to become more active in
     clearing the toxic protein buildup.

  2. Martorell AJ, et al. (2019). Cell, 177(2), 256-271.
     (cited above)

     Extended the approach to auditory and combined audio-visual
     stimulation. 40 Hz sound alone reduced amyloid plaques not just
     in sensory regions but also in the hippocampus and prefrontal
     cortex -- areas critical for memory. Mice showed improved
     performance on memory tasks.

  3. Chan D, Suk HJ, Jackson BL, et al. (2022). "Induction of specific
     brain oscillations may restore neural circuits and be used for the
     treatment of Alzheimer's disease." Journal of Internal Medicine,
     290(5), 993-1009.
     https://doi.org/10.1111/joim.13329

     Early human clinical trial data. Participants with mild
     Alzheimer's who received daily 40 Hz audio-visual stimulation
     showed reduced brain atrophy and improved functional connectivity
     compared to controls.

  4. He Q, Colon-Motas KM, et al. (2021). "A feasibility trial of
     gamma sensory flicker for patients with prodromal Alzheimer's
     disease." Alzheimer's & Dementia: Translational Research & Clinical
     Interventions, 7(1), e12178.
     https://doi.org/10.1002/trc2.12178

     Confirmed safety and tolerability of 40 Hz stimulation in human
     patients over extended periods.

HOW IT WORKS IN THE BRAIN

Gamma oscillations (30-100 Hz, centred around 40 Hz) are natural brain
rhythms associated with attention, perception, and memory. In
Alzheimer's disease, these oscillations are disrupted.

The 40 Hz auditory stimulation drives an Auditory Steady-State Response
(ASSR) -- the brain's auditory cortex locks onto the 40 Hz repetition
rate, producing strong gamma-frequency neural activity. This entrained
gamma activity appears to:

  - Activate microglia to clear amyloid-beta plaques and tau tangles
  - Improve cerebral blood flow and vascular function
  - Reduce neuroinflammation
  - Strengthen synaptic connections
  - Improve memory consolidation during sleep

The research is still ongoing. Cognito Therapeutics, an MIT spinoff,
is conducting FDA-approved clinical trials with a dedicated device.
This program replicates the auditory component of their protocol as
closely as possible using standard PC audio hardware.

IMPORTANT: This program is not a medical device and is not FDA-approved
for the treatment of any condition. The research is promising but still
in clinical trial stages. Consult a healthcare professional before using
this as part of any health regimen.
