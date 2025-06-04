---
Title: "Remote Assist Surgical Hand"
Authors: "Ruzanna Gaboyan and Philip Golczak"
Description: "For this project we are creating a robotic arm with a gripper hand which will be controlled by joystick and pneumatic haptic feedback system"
Created_At: "2025-05-28"
---
### May 27, 2025 - 2 hours

**What we did:**
- For our first work day on this project, we focused on brainstorming and research to get a clear picture of what we want to create. We discussed different project ideas that we had, some of which were focused on medical devices because of interest in biomedical engineering. After going through the pros, cons, and what the build process would look like for each concept, we decided to make a robotic hand with haptic feedback. The idea is that the hand could be useful in situations like small-scale surgeries or remote object handling where you need a sense of "touch" or grip resistance.
  
- One challenge we had was figuring out how to successfully achieve a functional haptic feedback system. After doing some research, we decided on trying to implement haptic feedback based on air pressure. The concept seems simple and effective. we’ll use a small tube with that you squeeze with your hand. The more you squeeze it, the more pressure it senses, and the robotic hand will close proportionally to that pressure. We want it tp work both ways. When the robotic hand feels resistance (like from gripping an object), it pushes air back into the tube, which increases the pressure and makes it physically harder to squeeze.
  
- To make the use of the arm easier, we decided to make it remotely controllable, and we are planning to use a joystick or remote control for that. Here is one useful article that we read to get an insight on how to build remotely controllable robot arms:
https://www.instructables.com/remote-controlled-robotic-arm/

- Here is a useful video that we watched which was also about building robotic arms:
https://youtu.be/5toNqaGsGYs?si=Z9oADLPzUGIuOL0S
(The robotic arm in this video is pretty big but ours will be smaller) 

### June 3, 2025 - 4 hours

**What we did:**
- We set up Visual Studio Code Git extension and cloned the project repository to our computers. We did this to make the workflow smoother since we plan to write the C code for our Arduinos in Visual Studio Code.


- We chose the mechanical parts such as the stepper and servo motors to drive the arm based on power requirements. We also chose our microcontroller allowing for Wi-Fi connectivity using an Arduino model for familiarity. Added these components to the Material and Budget file listing their prices and linking to possible vendors.

### June 4, 2025 - 1 hours

**What we did:**
- We began sketching out the CAD design on paper, dimensioning out the arms and making sure our servos can support the torque provided by the weight of the arm. This should allow for easier CADing in the future and smooth operation of the arm.