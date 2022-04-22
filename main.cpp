#include "mbed.h"
#include "rtos.h"
#include "MFRC522.h"
#include "uLCD_4DGL.h"

enum State_VM {
    idle = 0, paid, vending, unlocked, error_ISF, error_ICS
};

volatile int uses = 0;
volatile State_VM state = idle;
Mutex lcd;
char card[4] = {(char) 0x70, (char) 0xB8, (char) 0xDD, (char) 0x37};
char key[4] = {(char) 0xEA, (char) 0x4B, (char) 0x60, (char) 0xBF};

DigitalOut green(p19);
DigitalOut red(p20);
PwmOut left_motor(p21);
PwmOut right_motor(p22);
PwmOut lock_servo(p23);
DigitalOut left_button(p29);
DigitalOut right_button(p30);
MFRC522 pay_terminal(p5, p6, p7, p8, p16);
MFRC522 refill_terminal(p11, p12, p13, p14, p15);
uLCD_4DGL uLCD(p28, p27, p26);

void refill() {
    bool card_valid;
    refill_terminal.PCD_Init();
    while (1) {
        if (!refill_terminal.PICC_IsNewCardPresent()) {
            continue;
        }

        if (!refill_terminal.PICC_ReadCardSerial()) {
            continue;
        }

        card_valid = true;

        for (int i = 0; i < refill_terminal.uid.size; i++) {
            if (refill_terminal.uid.uidByte[i] != card[i]) {
                card_valid = false;
                break;
            }
        }

        if (card_valid) {
            uses++;
            green = 1;
            Thread::wait(1000);
            green = 0;
        } else {
            red = 1;
            Thread::wait(1000);
            red = 0;
        }
    }
}

void payment() {
    bool card_valid;
    bool key_valid;
    pay_terminal.PCD_Init();
    while (1) {
        if (!pay_terminal.PICC_IsNewCardPresent()) {
            continue;
        }

        if (!pay_terminal.PICC_ReadCardSerial()) {
            continue;
        }

        card_valid = true;
        key_valid = true;

        for (int i = 0; i < pay_terminal.uid.size; i++) {
            if (pay_terminal.uid.uidByte[i] != card[i]) {
                card_valid = false;
                break;
            }
        }
    
        if (!card_valid) {
            for (int i = 0; i < pay_terminal.uid.size; i++) {
                if (pay_terminal.uid.uidByte[i] != key[i]) {
                    key_valid = false;
                    break;
                }
            }
        }

        if (card_valid) {
            if (state != idle) continue;
            if (uses > 0) {
                uses--;
                state = paid;
            } else {
                state = error_ISF;
            }
        } else if (key_valid) {
            if (state == idle) {
                state = unlocked;
                lock_servo = 0.15f;
            } else if (state == unlocked) {
                state = idle;
                lock_servo = 0.05f;
            }
            Thread::wait(1000);
        } else {
            state = error_ICS;
        }
    }
}

void motors() {
    while (1) {
        while (state == idle) {}
        if (state == paid) {
            while (1) {
                if (left_button) {
                    state = vending;
                    left_motor = 0.4f;
                    break;
                }

                if (right_button) {
                    state = vending;
                    right_motor = 0.4f;
                    break;
                }
            }
        }

            Thread::wait(350);
            right_motor = 0;
            left_motor = 0;
    }
}

void screen() {
    while (1) {
        switch (state) {
            case idle:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(GREEN);
                uLCD.locate(1,2); 
                uLCD.printf("Please\n");
                uLCD.locate(1,3);
                uLCD.printf("make\n");
                uLCD.locate(1,4);
                uLCD.printf("payment.\n");
                while (state == idle) {}
                uLCD.cls();
                break;
            case paid:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(GREEN);
                uLCD.locate(1,2);
                uLCD.printf("Please\n");
                uLCD.locate(1,3);
                uLCD.printf("select\n");
                uLCD.locate(1,4);
                uLCD.printf("a snack\n");
                uLCD.locate(1,5);
                uLCD.printf("option.");
                while (state == paid) {}
                uLCD.cls();
                break;
            case vending:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(GREEN);
                uLCD.locate(1,2);
                uLCD.printf("Vending.\n");
                uLCD.locate(1,3);
                uLCD.printf("Please\n");
                uLCD.locate(1,4);
                uLCD.printf("wait.\n");
                Thread::wait(3000);
                state = idle;
                uLCD.cls();
                break;
            case unlocked:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(GREEN);
                uLCD.locate(0,2);
                uLCD.printf("Machine\n");
                uLCD.locate(0,3);
                uLCD.printf("is\n");
                uLCD.locate(0,4);
                uLCD.printf("unlocked.\n");
                while (state == unlocked) {}
                uLCD.cls();
                break;
            case error_ISF:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(RED);
                uLCD.locate(1,1);
                uLCD.printf("You\n");
                uLCD.locate(1,2);
                uLCD.printf("do not\n");
                uLCD.locate(1,3);
                uLCD.printf("have\n");
                uLCD.locate(1,4);
                uLCD.printf("enough\n");
                uLCD.locate(1,5);
                uLCD.printf("funds.\n");
                Thread::wait(3000);
                uLCD.cls();
                state = idle;
                break;
            case error_ICS:
                uLCD.text_height(2);
                uLCD.text_width(2);
                uLCD.color(RED);
                uLCD.locate(1,2);
                uLCD.printf("Please\n");
                uLCD.locate(1,3);
                uLCD.printf("use\n");
                uLCD.locate(1,4);
                uLCD.printf("a valid\n");
                uLCD.locate(1,5);
                uLCD.printf("card.\n");
                Thread::wait(3000);
                uLCD.cls();
                state = idle;
                break;
            default:
                uLCD.cls();
        }
    }
}

int main() {
    Thread t1;
    Thread t2;
    Thread t3;
    Thread t4;
    uLCD.baudrate(3000000);
    lock_servo.period(1.0/50);
    lock_servo = 0.05f;
    t1.start(refill);
    t2.start(payment);
    t3.start(motors);
    t4.start(screen);
    while(1) {}
}
