# Project_USBMouseDriver_Linux
Please watch video for reference: https://drive.google.com/file/d/1GseZiXURClr3eqDSLu5s_aME0oY1oQ1v/view?usp=sharing

In this project: We write a linux driver for mouse, read parameters such as: x, y position of mouse, this value is absolute, we take zero position at the top corner on the left (0,0). Moreover, it is included other things such as: number of mouse clikcs, wheels, speed as well as accurary of the mouse. After that, we save data in the text file and upload this data (with json char) to MQTT. We can receive this data by sub file and save them in SQL in our computer.

Thank you for your support.

Code team: Nguyen Nhat Phap Pham Ngoc Hieu  HCMUTE Instructor: Dr. Bui Ha Duc
