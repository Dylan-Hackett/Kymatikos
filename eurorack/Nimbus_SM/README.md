# Nimbus

## Author

Ported by Ben Sergentanis
Originally by Emilie Gillet

## Description

Nimbus is a port of Mutable Instrument's Clouds. Clouds is a granular  
audio processor, specializing in making huge clouds of sound from even the tiniest source.  

Ported from [pichenettes/eurorack](https://github.com/pichenettes/eurorack)

## TODO list for the Patch SM Port:
- Check if V/Oct CV input tracks properly
- Idea: Think of a function for CV1 Out (maybe Envelope Follower for Input Signal / VU meter)

## Controls
Every knob has two functions depending on the toggle switch. Only if the position of the knob matches the last stored value of the parameter the knob will control the parameter. This allows you to tweak only one setting when flipping the switch without the other parameters to be changed aswell. But starting the module you might need to twist the knobs to find the "last stored value".



<table><thead>
  <tr>
    <th>Control</th>
    <th colspan="2">Description</th>
    <th>Comment</th>
  </tr></thead>
<tbody>
  <tr>
    <td>Toggle Switch</td>
    <td>Switch Up</td>
    <td>Switch Down</td>
    <td>Toggling changes function of pots and button</td>
  </tr>
  <tr>
    <td>CV_1</td>
    <td>Position</td>
    <td>Pitch</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_2</td>
    <td>Size</td>
    <td>Dry/Wet</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_3</td>
    <td>Density</td>
    <td>Feedback</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_4</td>
    <td>Texture</td>
    <td>Reverb</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_5</td>
    <td colspan="2">V/Oct CV Input</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_6</td>
    <td colspan="2">Position CV Input</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_7</td>
    <td colspan="2">Density CV Input</td>
    <td></td>
  </tr>
  <tr>
    <td>CV_8</td>
    <td colspan="2">Texture CV Input</td>
    <td></td>
  </tr>
  <tr>
    <td>LED</td>
    <td>Show selected quality</td>
    <td>Show selected mode</td>
    <td>Indicated with brightness</td>
  </tr>
  <tr>
    <td>Button</td>
    <td>Cycle through qualities</td>
    <td>Cycle through modes</td>
    <td></td>
  </tr>
  <tr>
    <td>Gate In 1</td>
    <td colspan="2">Freeze Gate In</td>
    <td>Activates Freeze</td>
  </tr>
  <tr>
    <td>Gate In 2</td>
    <td colspan="2">Trigger In</td>
    <td>Triggers a Seed</td>
  </tr>
  <tr>
    <td>Gate Out x</td>
    <td colspan="2">Gate In x Through</td>
    <td>Both Gate Inputs are fed to outputs</td>
  </tr>
  <tr>
    <td>Audio In / Out 1 &amp; 2</td>
    <td colspan="2">1 = Left, 2 = Right</td>
    <td>Stereo in, stereo out</td>
  </tr>
</tbody></table>

Refer to the [Clouds Manual](https://mutable-instruments.net/modules/clouds/manual/) for more information on these controls.

#### Mode
You can select from Nimbus' four alternate modes here. These are:
- Granular
- Pitch Shift / Time Stretch
- Looping Delay
- Spectral Processor
Refer to the [Clouds Manual](https://mutable-instruments.net/modules/clouds/manual/) section on "The Infamous Alternate Modes".  

#### Quality
You can also select from four quality / mono stereo modes. These are:
- 16 bit Stereo
- 16 bit Mono
- 8bit u-law Stereo
- 8bit u-law Mono
Refer to the [Clouds Manual](https://mutable-instruments.net/modules/clouds/manual/) section on "Audio Quality".  
