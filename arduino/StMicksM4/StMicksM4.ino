#include <Servo.h>

#define STATE_STANDBY 1
#define STATE_COMPUTER_STARTING 2
#define STATE_HW_STARTING 3
#define STATE_RUNNING 4
#define STATE_COMPUTER_STOPPRING 5

#define EVENT_NOTHING 1
#define EVENT_AUDIO_ON 2
#define EVENT_AUDIO_OFF 3

//Controls servo to start Mac
const int servoPin = 23;

//Uses USB bus power to detect when Mac has actually started and shutdown
//Logic is inverted by opto-isolator
const int USBBusPowerPin = 5;

//Console switching power
//Logic is inverted by opto-isolator
const int switchingPowerPin = 10;

//Leds
const int ledStandbyPin = 22;
const int ledStartStopPin = 21;
const int ledRunPin = 20;

//Controls audio - probably via a contactor or relay - last on, first off
const int audioPowerPin = 17;

//Controls aux peripherals - follows USB bus power
const int auxPowerPin = 16;

//Shuts down Mac by sending a MIDI Program Change 127 on Channel 16.
const int midiChannel = 16;
const int midiShutdownPC = 127;
//Receieves MIDI PC to indicate if HW has started and stopped
const int midiAudioOnPC = 255;
const int midiAudioOffPC = 254;

Servo servo;

volatile byte state;
volatile byte event;

bool justTransitioned = false;

void setup() {
  Serial.begin(9600);

  initServo();

  usbMIDI.setHandleProgramChange(onProgramChange);

  pinMode(USBBusPowerPin, INPUT);
  pinMode(switchingPowerPin, INPUT);

  pinMode(audioPowerPin, OUTPUT);
  pinMode(auxPowerPin, OUTPUT);

  pinMode(ledStandbyPin, OUTPUT);
  pinMode(ledStartStopPin, OUTPUT);
  pinMode(ledRunPin, OUTPUT);

  switchOffAudio();
  switchOffAux();

  transitionTo(STATE_STANDBY);
}

void loop() {
  readMIDI();
  doStateMachine();
}

//The Main State Machine
void doStateMachine() {
  switch (state) {
    case STATE_STANDBY: {
        if (justTransitioned) {
          Serial.print("Standby\n");

          digitalWrite(ledStandbyPin, HIGH);
          digitalWrite(ledStartStopPin, LOW);
          digitalWrite(ledRunPin, LOW);

          justTransitioned = false;
        }

        if (digitalRead(switchingPowerPin) == LOW) {
          Serial.print("Switching Power ON\n");
          switchOnAux();
          delay(30000);
          switchOnMac();
          transitionTo(STATE_COMPUTER_STARTING);
        }
        break;
      }

    case STATE_COMPUTER_STARTING: {
        if (justTransitioned) {
          Serial.print("Waiting for Computer to Start\n");

          digitalWrite(ledStandbyPin, LOW);
          digitalWrite(ledStartStopPin, HIGH);
          digitalWrite(ledRunPin, LOW);

          justTransitioned = false;
        }

        if (digitalRead(USBBusPowerPin) == LOW) {
          Serial.print("USB Bus Power ON\n");

          transitionTo(STATE_HW_STARTING);
        }
        break;
      }

    case STATE_HW_STARTING: {
        if (justTransitioned) {
          Serial.print("Waiting for Hauptwerk to Start\n");

          justTransitioned = false;
        }
        if (event == EVENT_AUDIO_ON) {
          switchOnAudio();
          transitionTo(STATE_RUNNING);
        }
        break;
      }

    case STATE_RUNNING: {
        if (justTransitioned) {
          Serial.print("Hauptwerk Started\n");

          digitalWrite(ledStandbyPin, LOW);
          digitalWrite(ledStartStopPin, LOW);
          digitalWrite(ledRunPin, HIGH);

          justTransitioned = false;
        }

        if (digitalRead(switchingPowerPin) == HIGH) {
          Serial.print("Switching Power OFF\n");

          switchOffAudio();
          delay(10000);
          switchOffMac();

          transitionTo(STATE_COMPUTER_STOPPRING);
        }
        break;
      }

    case STATE_COMPUTER_STOPPRING: {
        if (justTransitioned) {
          Serial.print("Computer Stopping\n");

          digitalWrite(ledStandbyPin, LOW);
          digitalWrite(ledStartStopPin, HIGH);
          digitalWrite(ledRunPin, LOW);

          justTransitioned = false;
        }

        if (digitalRead(USBBusPowerPin) == HIGH) {
          Serial.print("Computer OFF\n");

          switchOffAux();

          transitionTo(STATE_STANDBY);
        }
        break;
      }
  }
}

void transitionTo(byte newState) {
  justTransitioned = true;
  state = newState;
  event = EVENT_NOTHING;
}

void readMIDI() {
  //Event handlers will be triggered by read().
  while (usbMIDI.read()) {
  }
}


//Hauptwerk needs to be programmed to send C7 FF when audio engine starts and C7 FE when it stops
void onProgramChange(byte channel, byte program) {
  if (channel == midiChannel) {
    if (program == midiAudioOnPC) {
      Serial.print("Audio Engine Started\n");
      event = EVENT_AUDIO_ON;
    } else if (program == midiAudioOffPC) {
      Serial.print("Audio Engine Stopped\n");
      event = EVENT_AUDIO_OFF;
    }
  }
}

void switchOnAudio() {
  digitalWrite(audioPowerPin, HIGH);
}

void switchOffAudio() {
  digitalWrite(audioPowerPin, LOW);
}

void switchOnAux() {
  digitalWrite(auxPowerPin, HIGH);
}

void switchOffAux() {
  digitalWrite(auxPowerPin, LOW);
}

void switchOnMac() {
  //Moves the servo to mechanically switch on Mac
  servo.attach(servoPin);
  servo.write(70);
  delay(200);
  servo.writeMicroseconds(1500);
  delay(200);
  servo.detach();
}

void switchOffMac() {
  usbMIDI.sendProgramChange(midiShutdownPC, midiChannel);
  delay(200);
  usbMIDI.sendProgramChange(midiShutdownPC, midiChannel);
  delay(200);
}

void initServo() {
  servo.attach(servoPin);
  servo.writeMicroseconds(1500);
  delay(200);
  servo.detach();
}


