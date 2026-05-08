# Virtual Assistant

## Requirements

- [ngrok](https://ngrok.com/)
- [nodejs](https://nodejs.org/en)
- npm package manager

## Installation

```sh
cd api ; npm i
npm start # starts localhost:8000 server, serves dashboard and api

# if you want to use the dashboard in your mobile devices
# in another terminal
npx ngrok http 8000
```

## Overview

The ESP32 Virtual Assistant is an integrated IoT system designed to manage personal tasks and provide environmental feedback. It combines hardware sensors and actuators with a cloud-connected backend and a web-based user interface.

## System Components

- **ESP32 Microcontroller**: Acts as the central hub, managing sensors, actuators, and communication with the server.
- **Web Dashboard**: Serves as the primary interface for user interaction, enabling voice commands and system configuration (toggling hardware feedback).
- **Node.js Backend**: Handles API requests, coordinates between the ESP32 and external services (like Google Calendar), and processes voice data.

## Key Features

1. **Voice Recognition**: Users can issue commands using their **phone microphone through the web ui dashboard**.
2. **Calendar Integration**: Fetches and displays upcoming events from Google Calendar on the OLED screen.
3. **Environment Monitoring**: Real-time temperature tracking using the DHT11 sensor.
4. **Physical Feedback**: 
   - **Servo Motor**: Provides kinetic feedback (e.g., waving) upon command completion.
   - **Buzzer**: Plays audio alerts and tones for system state changes.
5. **Interactive UI (OLED)**: Features custom animations, including "blinking eyes" for idle state and "jumping dots" for processing states.
6. **Remote Control**: The dashboard allows users to enable/disable specific hardware feedback (Servo and Buzzer) remotely.
7. **Bidirectional Sync**: Real-time bidirectional sync with Google Calendar ensures that events added or modified via voice are immediately reflected in the cloud and on the device.

### Flow Schema

```mermaid
flowchart TD
    A([Voice Input<br>Audio is captured via<br>phone microphone through the web ui dashboard])
    B[Process Voice Input<br>ESP32 records audio and sends WAV<br>to local server over LAN.<br>Server runs STT and LLM.]
    C{Select Task<br>Task is selected based on<br>the action field in LLM response}
    T1[Task 1<br>List Events<br>Calendar events are<br>listed on OLED]
    T2[Task 2<br>Set Reminder<br>An alarm is set<br>for the specified event]
    TN[Task N]
    D[Task Complete<br>Task is finished,<br>output is ready]
    E[Audio Output<br>A short beep is played<br>via buzzer]
    F[Visual Output on OLED<br>Character animation<br>and text is displayed]

    A --> B
    B --> C
    C -->|Calendar command| T1
    C -->|Reminder command| T2
    C -->|...| TN
    T1 --> D
    T2 --> D
    TN --> D
    D --> E
    D --> F
    E -. Simultaneously .-> F
    F -->|Repeat| A
```
