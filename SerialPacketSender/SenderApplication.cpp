//
//  SenderApplication.cpp
//  Error-Detecting Serial Packet Communications for Arduino Microcontrollers
//  Originally designed for use in the Office Chairiot Mark II motorized office chair
//
//  Created by Andy Frey on 4/13/15.
//  Copyright (c) 2015 Andy Frey. All rights reserved.


#include "SenderApplication.h"
#include "SerialPacket.h"


#define LED_SEND 13
#define LED_GOOD 12
#define LED_BAD 11
#define LED_DELAY() delayMicroseconds(100)


SenderApplication::SenderApplication() {}

void SenderApplication::_dumpStats(HardwareSerial *s) {
    uint32_t total = _stats.recv_good+_stats.send_bad+_stats.send_good+_stats.send_bad;
    uint32_t elapsed_secs = (millis() - _stats.start_time) / 1000;
    s->print("Send> Total:" + String(total, DEC));
    s->print(" Secs:" + String(elapsed_secs, DEC));
    s->print(" PPS:" + String(total/elapsed_secs, DEC));
    s->print(" RGood:" + String(_stats.recv_good, DEC));
    s->print(" RBad:" + String(_stats.recv_bad, DEC));
    s->print(" SGood:" + String(_stats.send_good, DEC));
    s->print(" SBad:" + String(_stats.send_bad, DEC));
    s->print(" SOoS:" + String(_stats.oos, DEC));
    s->println(" Acks:" + String(_stats.acks, DEC));
}

void SenderApplication::_newPacket() {
    _currentCommand.device = random(1, 254);
    _currentCommand.command = random(0, 255); // Use a reserved byte (FRAME_START) to test escaping
    _currentCommand.value = 100;
    _currentCommand.serial++;
    // 1 in 100 packets selected for ack request (set field to STATUS_NACK)
    _currentCommand.ack = random(1, 100) == 25 ? STATUS_NACK : STATUS_ACK;
}

/*
 *  Packet Delegate Method: Called when a valid packet is received
 */
void SenderApplication::didReceiveGoodPacket(SerialPacket *p) {
    _stats.recv_good++;
    // copy bytes for structure from packet buffer into structre memory
    Command _receivedCommand;
    memcpy(&_receivedCommand, p->buffer, p->getDataLength());
    if (_receivedCommand.ack == STATUS_ACK) {
        _stats.acks++;
        // this packet was properly acknowledged
        if (_receivedCommand.serial == _currentCommand.serial) {
            Serial.println("OK!");
        } else {
            _stats.oos++;
        }
    }
    _state = STATE_READY;

    digitalWrite(LED_GOOD, HIGH);
    LED_DELAY();
    digitalWrite(LED_GOOD, LOW);
}

/*
 *  Packet Delegate Method: Called when an error is encountered
 */
void SenderApplication::didReceiveBadPacket(SerialPacket *p, uint8_t err) {
    
    Serial.print("Error: ");
    switch (err) {
        case SerialPacket::ERROR_CRC      : Serial.println("CRC");      break;
        case SerialPacket::ERROR_LENGTH   : Serial.println("LENGTH");   break;
        case SerialPacket::ERROR_OVERFLOW : Serial.println("OVERFLOW"); break;
        case SerialPacket::ERROR_TIMEOUT  : Serial.println("TIMEOUT");  break;
        default                           : Serial.println("???");      break;
    }
    
    _stats.recv_bad++;
    _state = STATE_READY;

    digitalWrite(LED_BAD, HIGH);
    LED_DELAY();
    digitalWrite(LED_BAD, LOW);
}

/*
 *  Hard to tell, I know, but this is the main app loop.
 *  Sorry for the lack of self-documenting code. :(
 */
void SenderApplication::main() {

    pinMode(LED_SEND, OUTPUT);
    digitalWrite(LED_SEND, LOW);
    pinMode(LED_GOOD, OUTPUT);
    digitalWrite(LED_GOOD, LOW);
    pinMode(LED_BAD, OUTPUT);
    digitalWrite(LED_BAD, LOW);

    Serial.begin(115200);  // debugging
    Serial1.begin(230400); // packets

    SerialPacket p;
    p.setDelegate(this);
    p.use(&Serial1);

    _currentCommand.serial = 0;

    _state = STATE_READY;

    Serial.println("GO!");
    _stats.start_time = millis();

    while (1) {

        // give packet time to receive any incoming data
        p.loop();

        if (_state == STATE_READY) {
            // send a packet
            digitalWrite(LED_SEND, HIGH);
            _newPacket();
            uint8_t bytesSent = p.send((uint8_t *)&_currentCommand, sizeof(_currentCommand));
            if (bytesSent > 0) {
                _stats.send_good++;
                digitalWrite(LED_GOOD, HIGH);
                LED_DELAY();
                digitalWrite(LED_GOOD, LOW);
                if (_currentCommand.ack == STATUS_NACK) {
                    Serial.print("Waiting for ACK... ");
                    _state = STATE_WAIT_ACK;
                }
                if ((_stats.send_good > 0) && (_stats.send_good % 1000 == 0)) {
                    _dumpStats(&Serial);
                }
            } else {
                _stats.send_bad++;
                digitalWrite(LED_BAD, HIGH);
                LED_DELAY();
                digitalWrite(LED_BAD, LOW);
            }
            digitalWrite(LED_SEND, LOW);
        }
    }

}
