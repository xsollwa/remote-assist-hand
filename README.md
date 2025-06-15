# remote-assist-hand
A robotic hand with pressure sensing and haptic feedback communication

**Created by:** Ruzanna Gaboyan and Philip Golczak  
**Date:** May 28, 2025  

## Overview
The **Remote Assist Hand** is a robotic arm with a gripping hand that is able to sense how much pressure it feels when gripping and holding objects. It uses a load cell to measure this force and sends that information to a computer through an ESP32 microcontroller. 

The main idea of our project is to *communicate the "feeling of touch"* while controlling the robot, especially for tasks that demand percision when it comes to pressure awareness: like when handling delicate objects. Unlike most robotic arms, **Remote Assist Hand** can *sense physical resistance and communicate it back* to the user, which helps them to know how hard the robot is gripping without directly touching anything. 

## Features
- **Force sensing with load cell:** Senses how much pressure the hand feels when it grabs an object.
- **Real time feedback to computer**: ESP32 wirelessly sends data from the hand to a computer.
- **Remote control using computer**: The arm is designed to be operated remotely through a computer. 
- **Custom PCB and 3D design**: All parts including the base, casing and PCB are custom designed and printed

## Components
| Part        | Description            |
|-------------|-------------------------|
|ESP32|Microcontroller for communication through Wi-Fi|
|Load Cell with an Amplifier|Measures grip force|
|Servo Motors 4x|Move the arm joints|
|Stepper Motor|Rotates the base|
|PCB|Handles power delivery and connections|
|LiPo Battery|Powers the arm|
|3D printed parts|Arm structure, base and electronics casing|
|Voltage regulators and buck converters|Maintain correct voltage|

You can also view our full Bill of Materials [here](./MATERIALS-AND-BUDGET.md)

## How it Works
1) Servo motors (controlled from a computer) move the arm and the gripper to **hold an object**,
2) The load cell inside the gripper **measures how much force** is being applied,
3) ESP32 **sends the data to a computer** in real time,
4) The computer **displays the pressure**, helping the user understand how hard the robot is gripping.

## Building Process
1) Brainstorming and sketching
2) PCB Design using KiCAD 
3) CAD Modeling using OnShape
4) Revision and Optimization

## Journal
Our full development journal can be found [here](./JOURNAL.md)

## Contact
For any questions or suggestions please reach out to us at *gaboyanruzanna@gmail.com* or *ph.golczak@gmail.com* .
