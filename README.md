# Delexander Volume 1

**Volume 1** is a plugin for [VCV Rack](https://github.com/VCVRack/Rack). Two modules are included:
* Algomorph Pocket
* Algomorph Advance

Both modules are designed principally for use in FM synthesis; each can be used to take place of the "algorithm" section typical of FM synthesizers.

![Algomorph and Algomorph Pocket](<res/Algomorph_SoloImage.png>)

**Algomorph Pocket** and **Advance** are capable of storing 3 routing states (algorithms) and crossfading between them via knob or control voltage.

Both modules feature intelligent routing which automatically assigns an operator to the *Carrier* output if that operator is not routed to any other operators.

Both modules also feature a display for graph visualization. The display contains 1980 hardcoded algorithms which correspond to a set typical for FM synthesizers: it is able to display any algorithm which contains at least one natural carrier. This display is also capable of crossfading between algorithms.

When being used for FM synthesis, pairing with oscillators (operators) capable of linear FM is recommended. For example:
* Bogaudio [FM-OP](https://library.vcvrack.com/Bogaudio/Bogaudio-FMOp)
* Fundamental [WT-VCO](https://library.vcvrack.com/Fundamental/VCO2)
* KauntenjaDSP [Mini Boss](https://library.vcvrack.com/KautenjaDSP-PotatoChips/MiniBoss)
* NYSTHI [µOPERATOR](https://library.vcvrack.com/NYSTHI/OP)/[TZOP](https://library.vcvrack.com/NYSTHI/TZOP)
* RPJ [Pigeon Plink](https://library.vcvrack.com/RPJ/PigeonPlink)
* Squinky Labs [Kitchen Sink](https://library.vcvrack.com/squinkylabs-plug1/squinkylabs-wvco)
* Submarine [PO-204](https://library.vcvrack.com/SubmarineFree/PO-204)
* Valley Audio [Terrorform](https://library.vcvrack.com/Valley/Terrorform)

In addition to the quickstart guide included below, there is a video overview of the basics available from Jakub Ciupinski on his YouTube channel:
https://www.youtube.com/watch?v=AhxCv6AoOt4

# Quickstart:
* Connect the output of up to four operators to the Operator Inputs. Configure their modulation-depths and frequency ratios.   
* Connect the Modulation Outputs of Algomorph to the linear FM inputs of the operators.  
* Connect the Carrier Sum output to your audio device or a mixer.  
* Press an Operator Button followed by a Modulation Button to connect one operator to another. Repeat until you have built a desired algorithm.  
* After you have finished building your algorithm, press Algorithm Button 1 or 3 to build a new algorithm.
* Once finished, press the Edit button to exit Edit Mode 
* The Morph knob allows for crossfading between the stored algorithms. 12 o'clock is the currently selected algorithm, while 7 o'clock is one algorithm to the left and 5 o'clock is one algorithm to the right.  
* The CV input can also be used in addition to the knob, accepting +/- 5V
* Connecting an operator to its own modulation output will disable that operator, silencing its output and removing it from the Sum output for that algorithm.
* To force an operator to act as carrier even when it is acting as a modulator, press its corresponding modulator button while the operator is not selected. A rotating indicator light will confirm the operator is now a designated carrier for the current algorithm.
* To randomize only the algorithms or only a single algorithm, and not any of the knobs, right click on the connection area to find the Algorithm Randomization menu.

# Acknowledgement

The display graphics are generated using a combination of dot-language enumeration of the problem space, rendering to SVG via [GraphViz](https://graphviz.org/), and conversion to CSV with [Beautiful Soup](https://www.crummy.com/software/BeautifulSoup/).

Thanks go out to the all the developers, educators, and artists who have contributed to the VCV Rack and Eurorack communities. Without their work, Algomorph would not be possible.
