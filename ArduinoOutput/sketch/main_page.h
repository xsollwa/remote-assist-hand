#line 1 "C:\\Hapatic_Feedback\\remote-assist-hand\\Code\\VICTOR\\main_page.h"
#pragma once
#include <pgmspace.h>

const char MAIN_PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <title>Remote Assist Hand Controls</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    .button {
      position: relative;
      display: inline-block;
      width: 120px;
      height: 68px;
      line-height: 68px;
      vertical-align: middle;
      margin: 10px;
      border: 2px solid transparent;
      border-radius: 6px;
      background-color: #034078;
      color: white;
      font-size: 1.2rem;
      text-align: center;
      cursor: pointer;
      transition: background-color 0.2s, color 0.2s;
      user-select: none;
      outline: none;
    }
    .button:hover {
      background-color: #022f54;
    }
    .button:active {
      background-color: white;
      color: #034078;
      border-color: #034078;
    }
    html {
      font-family: Arial, Helvetica, sans-serif;
      text-align: center;
    }
    body {
      margin: 0;
    }
    .topnav {
      background-color: #034078;
    }
    h1 {
      font-size: 1.8rem;
      color: white;
      margin: 0;
      padding: 16px;
    }
    .content {
      padding: 50px;
    }
    .card-grid {
      display: grid;
      grid-gap: 2rem;
      grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
      justify-items: center;
    }
    .card {
      background-color: white;
      box-shadow: 2px 2px 12px rgba(140,140,140,0.5);
      border-radius: 8px;
      padding: 20px;
      width: 100%;
      max-width: 320px;
    }
    .card-title {
      font-size: 1.2rem;
      font-weight: bold;
      color: #034078;
      margin-bottom: 15px;
    }
    #force-display {
      font-size: 1.5rem;
      color: #034078;
      margin-bottom: 20px;
    }
  </style>
</head>
<body>
  <div class="topnav">
    <h1>Remote Assist Hand Controls</h1>
  </div>
  <div class="content">
    <div id="force-display">Force: 0 N</div>
    <div class="card-grid">
      <div class="card">
        <p class="card-title">Base Stepper</p>
        <button class="button" onmousedown="startMotor('base_stepper','ccw')" onmouseup="stopMotor('base_stepper')" onmouseleave="stopMotor('base_stepper')">CCW</button>
        <button class="button" onmousedown="startMotor('base_stepper','cw')"  onmouseup="stopMotor('base_stepper')" onmouseleave="stopMotor('base_stepper')">CW</button>
      </div>
      <div class="card">
        <p class="card-title">Shoulder Servo</p>
        <button class="button" onmousedown="startMotor('shoulder_servo','ccw')" onmouseup="stopMotor('shoulder_servo')" onmouseleave="stopMotor('shoulder_servo')">CCW</button>
        <button class="button" onmousedown="startMotor('shoulder_servo','cw')"  onmouseup="stopMotor('shoulder_servo')" onmouseleave="stopMotor('shoulder_servo')">CW</button>
      </div>
      <div class="card">
        <p class="card-title">Elbow Servo</p>
        <button class="button" onmousedown="startMotor('elbow_servo','ccw')" onmouseup="stopMotor('elbow_servo')" onmouseleave="stopMotor('elbow_servo')">CCW</button>
        <button class="button" onmousedown="startMotor('elbow_servo','cw')"  onmouseup="stopMotor('elbow_servo')" onmouseleave="stopMotor('elbow_servo')">CW</button>
      </div>
      <div class="card">
        <p class="card-title">Wrist Servo</p>
        <button class="button" onmousedown="startMotor('wrist_servo','ccw')" onmouseup="stopMotor('wrist_servo')" onmouseleave="stopMotor('wrist_servo')">CCW</button>
        <button class="button" onmousedown="startMotor('wrist_servo','cw')"  onmouseup="stopMotor('wrist_servo')" onmouseleave="stopMotor('wrist_servo')">CW</button>
      </div>
      <div class="card">
        <p class="card-title">Grasper Servo</p>
        <button class="button" onmousedown="startMotor('grasper_servo','ccw')" onmouseup="stopMotor('grasper_servo')" onmouseleave="stopMotor('grasper_servo')">CCW</button>
        <button class="button" onmousedown="startMotor('grasper_servo','cw')"  onmouseup="stopMotor('grasper_servo')" onmouseleave="stopMotor('grasper_servo')">CW</button>
      </div>
    </div>
  </div>
  <script>
    const gateway = `ws://${window.location.hostname}/ws`;
    let websocket;
    const intervals = {};

    window.addEventListener('load', initWebSocket);
    function initWebSocket() {
      websocket = new WebSocket(gateway);
      websocket.onopen = () => console.log('WebSocket connected');
      websocket.onclose = () => { console.log('WebSocket disconnected'); setTimeout(initWebSocket, 2000); };
      websocket.onmessage = onMessage;
    }

    function onMessage(event) {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'force' && data.motor === 'grasper_servo') {
          document.getElementById('force-display').textContent = `Force: ${data.force.toFixed(2)} N`;
        } else {
          console.log('Received:', data);
        }
      } catch (e) {
        console.log('Non-JSON message:', event.data);
      }
    }

    function sendMotorCommand(motor, dir) {
      if (!websocket || websocket.readyState !== WebSocket.OPEN) return;
      const cmd = { type: 'move', motor: motor, dir: dir };
      websocket.send(JSON.stringify(cmd));
    }

    function startMotor(motor, dir) {
      sendMotorCommand(motor, dir);
      if (intervals[motor]) clearInterval(intervals[motor]);
      intervals[motor] = setInterval(() => sendMotorCommand(motor, dir), 200);
    }

    function stopMotor(motor) {
      if (intervals[motor]) {
        clearInterval(intervals[motor]);
        delete intervals[motor];
      }
      const cmd = { type: 'stop', motor: motor };
      if (websocket && websocket.readyState === WebSocket.OPEN) websocket.send(JSON.stringify(cmd));
    }
  </script>
</body>
</html>
)rawliteral";
