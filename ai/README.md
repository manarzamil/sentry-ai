# Fatigue Analytics

## What this module is

The fatigue subsystem is **rule-based sensor analytics**, implemented in `runAI()` in the firmware. It computes descriptive statistics over a rolling window of accelerometer magnitudes and combines them with tilt into a weighted score.

**It is not machine learning.** There is no trained model, no dataset, no inference engine and no learned parameters anywhere in this repository. Every threshold and weight was chosen by hand. This distinction is stated plainly because describing a deterministic scoring function as machine learning would misrepresent the work.

## Inputs

A circular buffer of 50 acceleration-magnitude samples is filled by the main loop at roughly 50 Hz, giving a window of approximately one second. Scoring runs only when the buffer is full and at most once every 2500 ms.

| Feature | Computation | Interpretation |
|---------|-------------|----------------|
| Mean activity | Average of `abs(magnitude - 9.81)` across the buffer | How much the worker is moving; low values indicate inactivity |
| Variance | Variance of that same deviation series | How irregular the movement is; near-zero suggests an unnaturally static posture |
| Tilt | Current angle from vertical | Sustained head droop |

## Scoring

| Condition | Contribution |
|-----------|--------------|
| Mean activity below 1.18 | Up to **40 points**, scaled by how far below the threshold it falls |
| Tilt above 25° | Up to **35 points**, scaled linearly over the following 20° and clamped |
| Variance below 0.05 | A flat **25 points** |

The total is clamped to 0–100.

## Alert logic

Scores above 60 add 2 to a persistence counter each evaluation; anything lower decrements it by 1, so a sustained condition is required rather than a single reading. When the counter reaches 30 an alert fires, sounding buzzer pattern 2 and writing a log entry containing the contributing values. A 120-second cooldown prevents repeat alerts, and the alert clears once the score falls below 30. The asymmetric increment and decrement mean the system reacts faster to developing fatigue than it does to recovery, which is the appropriate bias for a safety warning.

## Honest limitations

The thresholds are unvalidated. They were selected by hand and have not been checked against any labelled dataset or physiological ground truth, so the score should be read as a coarse indicator of prolonged inactivity in a drooped posture rather than a clinical measure of fatigue.

The features are also inherently ambiguous. A worker concentrating on precise stationary work will look identical to a fatigued worker, because both produce low activity, low variance and forward head tilt. Nothing in the current feature set separates the two.

Finally, there is no personalisation. Fixed thresholds are applied identically to every worker regardless of role, build or task, and no baseline period is used to calibrate what "normal" looks like for an individual.

## What a genuine ML implementation would require

Moving from heuristics to machine learning would need labelled multi-hour recordings from real workers with fatigue ground truth from a validated instrument, expanded features such as gait regularity and micro-movement frequency, and a model small enough to run under TensorFlow Lite for Microcontrollers within the ESP32's memory budget. Evaluation would have to be subject-independent, holding out entire workers rather than shuffling windows, since shuffling leaks near-identical samples between train and test and inflates apparent accuracy. Per-worker calibration during a supervised baseline period would likely matter more than model architecture.

This is described as future work. None of it is implemented.
