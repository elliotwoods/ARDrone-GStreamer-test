ARDrone knowledge

# Glossary 

## I frames and P frames

###Intra frames 
Intra frames (sometimes called _keyframes_), are full image frames. A decoder can output an image using any single intraframe.

###Predicted frames
Predicted frames denote the change in image from the previous frame. For a decoder to output the image from a P frame, it must decode all previous frames up to and including the most recent I frame (e.g. I>P>P>P would give the image of the 4th frame using the I frame and using the cumulative information of the following 3 frames).

Predicted frames are useful for reducing bandwidth requirements as P frames generally require less data than I frames. However, having a long string of P frames will lead to image degredation as the errors are accumulated, and seeking videos with many P frames is difficult, as when a seek is performed, all data up to the previous I frame must be integrated together to produce the output image. Commonly there may be an I frame for every 10 − 100 P frames.
 
## P frames
See _I frames and P frames_ above.

## PaVE
Parrot Video Encapsulation is a header format for transmitting video.

## P264
P264 is a h264 stream encapsulated with a PaVE header. Using P264 it’s possible to omit P frames whenever a new I frame is available (thereby reducing latency).

