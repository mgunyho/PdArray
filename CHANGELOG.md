# Changelog for PdArray

## v2.1.0 (unreleased)

Fairly large update to Miniramp.

- New outputs: EOC (End of Cycle) and FINISH (inverse of GATE). This makes chaining Arrays easier (thanks @davephillips for the feature suggestion).
- New input: STOP / reset
- Miniramp is now wider, to fit the new input and outputs
- Polyphonic behavior of Miniramp is now consistent with that of Ministep: if either the trigger or CV input are polyphonic, the output is polyphonic as well, and if the trigger, CV or STOP inputs are monophonic, they affect all channels
- New right click menu option to update trigger duration only when a trigger is received
- Duration and CV amount knob tooltips now show the duration / CV amount correctly in seconds
- Extended Miniramp manual to explain polyphony

Additional changes:

- Updated screenshots in manual to Rack V2

## v2.0.6 (2021-12-29)

Ported to Rack v2.

- Implement v2 API features, such as input/output labels, and improved labels for switches. Also, glow-in-the-dark array cursors.
- If the array contains more than 5000 elements, the array data is saved as a wav file in the patch storage directory
- Improvements to the array size and Ministep step count text boxes

As part of the v2 update, PdArray is now licensed under the EUPL.

## v1.0.6 (2020-11-14)

- Fix crash when loading a sampe without resizing ([#13](https://github.com/mgunyho/PdArray/issues/13))
- Add right-click option to set array contents to zero
- Add "Add fade in/out" option to array to prevent clicks ([#14](https://github.com/mgunyho/PdArray/issues/14))
- Add option to keep Miniramp value at 10V after the ramp has finished
- Add section on clicking prevention in manual

## v1.0.5 (2020-06-19)

- Added option to enable recording either with a gate or trigger signal
- Added option to sort the array ([#11](https://github.com/mgunyho/PdArray/issues/11))

## v1.0.4 (2020-06-11)

- Add option to resize Array when loading an audio file ([#9](https://github.com/mgunyho/PdArray/issues/9))
- Add "Data persistence" option - it's now possible to avoid saving the array data to the patch file, or to automatically reload an audio file when the patch is loaded
- Fixed issue with Ministep display (for real this time) ([#8](https://github.com/mgunyho/PdArray/issues/8))

## v1.0.3 (2020-01-25)

- Added scale parameter to Ministep

## v1.0.2 (2019-11-03)

- Added the Ministep module (based on ideas in [#6](https://github.com/mgunyho/PdArray/issues/6))
- Fixed a bug in indexing, Array now properly works as a sequencer ([#7](https://github.com/mgunyho/PdArray/issues/7))
- Array size can now be edited also by right-clicking ([#4](https://github.com/mgunyho/PdArray/issues/4))
- Fix a typo ([#5](https://github.com/mgunyho/PdArray/issues/5))
- Update POS and I/O range tooltips
