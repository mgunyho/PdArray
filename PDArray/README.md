# PDArray

This is a VCV Rack module that allows creating custom envelopes, much like the
`array` object in [Pure Data](https://puredata.info/). You can enter values into
the array either by manually drawing, recording an input, or loading a sample. A
CV input controls the position of a cursor for reading out the array values. The
output can either read the stepped values of the array directly or smooth
interpolated values, using the same interpolation algorithm as PD.

The plugin also contains a small module called Miniramp to easily record or
read out the array sequentially.

