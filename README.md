# touchdesigner-atem-chop

## Overview
Control Blackmagic Design ATEM from TouchDesigner's CHOP Operator.

## How to use

### Start
Put the .dll file in the Plugins folder that you created on the same level as the TouchDesigner project file.
Start TouchDesigner and select Atem CHOP from the Custom tab of the Operator Selector.
Just enter Atem's IP address in Atem CHOP to complete the connection.

### Get the Atem's Status
You can get the status from the output of Atem CHOP.
Information such as the number of inputs and macros can be obtained through Info CHOP, and input names and macro names can be obtained through Info DAT.

### Operate the ATEM
To operate the ATEM, put the data into the input.
To change the program, enter the data with the channel name cpgi1.
The name cpgi1 probably stands for change program input M/E1.
To operate M/E2, it is cpgi2.
To change the preview, it will be cpvi1.
Similarly, to manipulate the preview of M/E2, it is cpvi2.

To perform CUT/AUTO, send the PULSE signal.
To send CUT to M/E1, the channel name is dcut1.

### Commands List
| CMD | Description | |
| --- | --- | --- |
| cpgi1 | Set Program Input | value is source index |
| cpvi1 | Set Preview Input | value is source index |
| dcut1 | Cut | pulse |
| daut1 | Auto Transition | pulse |
| caus1 | Set Aux Input | value is source index |
| cdsl1 | Set Downstream Keyer | value is On(1) or Off(0) |
| ddsa1 | Downstream Keyer Auto Transition | pulse |

The 1 at the end of the command is the index of the M/E.
When operating 2nd M/E, change that to 2.

## Tutorial Movie
https://youtu.be/dTgaoj8VZiA

## Reference
https://www.blackmagicdesign.com/products/atem
