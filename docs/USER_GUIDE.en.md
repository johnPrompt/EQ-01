# EQ-01 user guide

Install the complete bundle using the [platform instructions](../README.md#download-and-install). Public downloads contain the VST3 plug-in.

## Graph and band parameters

Hover previews a proposed filter. With no selection, click to add a band; with a selection, clicking empty space deselects it. Double-click empty space or Ctrl+click to add directly. Graph edges suggest cuts/shelves and the centre suggests Bell. Change the shape in the band panel.

Drag a point to change frequency/gain; Shift gives finer movement. The wheel changes Q or, on cuts, slope. Adding on the summed curve starts at zero gain. Single-click selects and leaves the graph free. Double-click a point or choose Parameters from its context menu to open details.

Move the panel by its title or empty background. Parameter changes preserve its position. X/Escape closes it while keeping selection. Selecting another band closes it. During a graph drag the panel temporarily hides and returns if it was explicitly open.

Numbers support horizontal/vertical dragging, Shift precision, wheel and arrow keys. Double-click or Enter opens text entry; frequencies accept `4.1k` and comma decimals. Each drag is one automation gesture and one undo step.

Shape sets the filter, Frequency positions it, Gain boosts/cuts and Q controls width. Cuts use Slope instead of Gain; their Q is hidden at 24 dB/oct or more. Notch, Band-pass and All-pass have no Gain. Channel selects Stereo, Left, Right, Mid or Side.

## Dynamics, resonances and sidechain

Dynamics Off is fixed EQ. Dynamics changes the whole band with level. Resonances adjusts narrower regions and retains static EQ gain. Threshold and Above range are primary: negative range attenuates; positive boosts. Timing & response reveals attack, release, ratio, knee, Below range and Relative. Detector reveals independent frequency/Q. Relative reduces sensitivity to overall input level.

Choose Own signal or External SC and connect the external input in your DAW. An unconnected input gives silence; all parameters remain editable. A nonzero Below range can act on silence. Independent frequency/Q sets the detector separately from the EQ region.

Headphones audition the band region or detector, including external SC. Audition is temporary; changing selection or closing the editor restores normal monitoring. Bypass takes priority. Expanded/auditioned Detector shows a dotted SC spectrum before its frequency filter. Stereo energy is combined, and the display adds no audio delay.

Density controls Live filter-bank selectivity or Spectral frequency smoothing. Different algorithms need their own threshold tuning.

## Compare, presets and shortcuts

Compare with original stays visible in the header. The bottom bar holds engine/delay, colour, output, Sketch, spectrum freeze and difference monitoring. Freeze stops the image. Difference monitors processing minus the delay-aligned original. The gear holds drive, width, A/B and copying from another same-process instance.

Presets save as `.eq01preset`; older `.cspreset` files remain readable. EN/RU is stored in the project. A/B and undo history currently last while the editor is open. Right-click a band for parameters, audition, copying and deletion. Delete removes the selected band. Ctrl+C/Ctrl+V copies/pastes; Ctrl+Z/Ctrl+Shift+Z undoes/redoes. Sketch fits up to eight Bell extrema and does not reproduce arbitrary curves exactly.

## Latency and limits

Clean + Live adds zero samples of buffer delay, including dynamics and causal resonances. Filter phase, detector attack and device latency still apply. Spectral uses 2048-point STFT / 512 hop and adds 2048 samples; conventional EQ filters remain minimum phase.

Subtle/Warm colour uses saturation and 2× IIR oversampling with a small delay. Drive controls intensity. No specific hardware is emulated. Change engine/colour while stopped so the host can update delay compensation.

Output has no limiter. Version 0.3.2 is a development alpha. See [validation](VALIDATION.md) for measured checks and open DAW testing.
