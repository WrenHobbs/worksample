/* 
    Author:     Wren Hobbs
    Class:      CSE 321
    Program:    Braille-to-Morse communication device
    Created:    11/25/2021
    Last Edited: 12/9/2021
    Details:    This system allows remote tactile communication between two parties.
    A user can enter text via a braille keyboard, and the system will output the text
    as morse code through a vibration motor.
*/
#include "mbed.h"

Mutex msglock;      //protexts msg and len variables
Mutex inputlock;    //used to protect input variable
Mutex enterlock;    //used to protect enter ISR from bounce
Mutex backspacelock;//used to protect backspace ISR from bounce
Mutex sendlock;     //used to protect send ISR from bounce

ConditionVariable enterCond(enterlock);         //used to protect enter ISR from bounce
ConditionVariable backspaceCond(backspacelock); //used to protect backspace ISR from bounce
ConditionVariable sendCond(sendlock);           //used to protect send ISR from bounce

InterruptIn enterButton(PB_15, PullDown);       //button used for enter ISR
InterruptIn backspaceButton(PB_13, PullDown);   //button used for backspace ISR
InterruptIn sendButton(PB_12, PullDown);        //button used for send ISR

BusIn bumps(PC_12, PC_11, PC_10, PC_9, PC_8, PC_6); //input bus for bump buttons.
//From highest to lowest bit: bottom right, middle right,  top right, bottom left, middle left, top left.

DigitalOut vcc(PD_0);   //output always on for button input

DigitalOut braillevibrate(PE_14);   //vibration motor used for braille keyboard UI
DigitalOut morsevibrate(PF_8);      //vibration motor used for morse code output

Thread isrThread(osPriorityHigh);           //thread that handles the ISR Event Queue
Thread enterThread(osPriorityNormal);       //thread that handles the enter button's functionality
Thread backspaceThread(osPriorityNormal);   //thread that handles the backspace button's functionality
Thread sendThread(osPriorityNormal);        //thread that handles the send button's functionality

EventQueue queue(64*EVENTS_EVENT_SIZE);     //Event Queue used by the ISRs

char msg[140] = {};     //holds a string of up to 140 characters, written by inputs, read by output
int len = 0;            //keeps track of the length of the message stored in msg
int input = 0;          //used to read the bump button bus; 6-bit integer

void enter();           //function that the enter thread runs
void backspace();       //function that the backspace thread runs
void send();            //function that the send thread runs

void enterISR();        //ISR called when enter button is pressed
void backspaceISR();    //ISR called when backspace button is pressed
void sendISR();         //ISR called when send button is pressed

void brailleToText();   //reads the input variable, and writes the appropriate character to the msg string
void dot();             //outputs a morse code "dot" to the morse vibration motor
void dash();            //outputs a morse code "dash" to the morse vibration motor
void printMorse();      //reads msg string and outputs it as morse code to the vibration motor

//main(): initializes and starts threads and ISRs,
//Initializes watchdog, periodically kicks the watchdog.
int main() {
    vcc = 1;    //enable vcc to always be high

    RCC->AHB2ENR |= 2;  //enable port B
    //set PB_1 as output
    GPIOB->MODER |= 4;
    GPIOB->MODER &= ~(8);

    bumps.mode(PullDown);   //set bump bus to pull down

    Watchdog &watchdog = Watchdog::get_instance();  //initialize watchdog

    watchdog.start(8000);   //start the watchdog. If the watchdot is not kicked for 8 seconds, system is restarted.

    enterButton.rise(queue.event(&enterISR));           //set enter button as interrupt
    backspaceButton.rise(queue.event(&backspaceISR));   //set backspace button as interrupt
    sendButton.rise(queue.event(&sendISR));             //set send button as interrupt

    enterThread.start(enter);           //start thread for enter button functionality
    backspaceThread.start(backspace);   //start thread for backspace button functionality
    sendThread.start(send);             //start thread for send button functionality

    isrThread.start(callback(&queue, &EventQueue::dispatch_forever));   //set ISR thread to handle the event queue
    
    //Loop forever
    while(1){
        watchdog.kick();            //kick watchdog
        ThisThread::sleep_for(1s);  //sleep for 1 second; 1/8 of the watchdog's timeout
    }
    return 0;
}

//enter(): run forever by enterThread. When it receives a signal by the enterISR,
//it will write the bump bus' current state to input and call brailleToText()
void enter() {
    //loop forever
    while (1) {
        enterlock.lock();       //lock enter mutex to protect from bounce   
        enterCond.wait();       //wait for a signal from ISR
        inputlock.lock();       //lock input mutex
        ThisThread::sleep_for(150ms);   //sleep for 150 milliseconds to account for bounce on the bump buttons
        input = bumps.read();   //write current state of bump buttons to input
        inputlock.unlock();     //unlock input mutex
        brailleToText();        //call function that converts input to character and adds it to msg
        printf("message: \"%s\"\n", msg);   //print current message on the console
        enterlock.unlock();     //unlock enter mutex
    }
}

//backspace(): run forever by backspaceThread. When it receives a signal by the backspaceISR,
//it will delete the most recently added character from the msg string.
void backspace() {
    //loop forever
    while (1) {
        backspacelock.lock();   //lock backspace mutex to protect from bounce
        backspaceCond.wait();   //wait for a signal from the ISR
        msglock.lock();         //lock msg mutex
        if (len > 0) {          //if msg is not empty:
            len--;              //reduce the length of msg by 1
            msg[len] = '\0';    //set erased character to ASCII null
        }
        printf("message: \"%s\"\n", msg); //print current message on the console   
        msglock.unlock();               //unlock msg mutex
        braillevibrate = 1;             //enable keyboard motor to notify user it received the input
        ThisThread::sleep_for(100ms);   //sleep for 100ms
        braillevibrate = 0;             //disable keyboard motor
        ThisThread::sleep_for(100ms);   //sleep for another 100ms to prevent bounce
        backspacelock.unlock();         //unlock backspace mutex
    }
}

//send(): run forever by sendThread.When it receives a signal from the sendISR,
//it will call printMorse, which outputs the stored message as morse code and erases the message.
void send() {
    //loop forever
    while (1) {
        sendlock.lock();    //lock send mutex to protect from bounce
        sendCond.wait();    //wait for a signal from the ISR
        sendlock.unlock();  //unlock send mutex
        printMorse();       //call function to output the message as morse code
    }
}

//enterISR(): When the enter button is pressed, this ISR will signal enterThread.
void enterISR() {
    enterlock.lock();       //lock the enter mutex so it can signal
    enterCond.notify_all(); //signal enterThread
    enterlock.unlock();     //unlock enter mutex
}

//backspaceISR(): When the backspace button is pressed, this ISR will signal backspaceThread.
void backspaceISR() {
    backspacelock.lock();       //lock the backspace mutex so it can signal
    backspaceCond.notify_all(); //signal backspaceThread
    backspacelock.unlock();     //unlock backspace mutex
}

//sendISR(): When the send button is pressed, this ISR will signal sendThread.
void sendISR() {
    sendlock.lock();        //lock the send mutex so it can signal
    sendCond.notify_all();  //signal sendThread
    sendlock.unlock();      //unlock send mutex
}

void brailleToText() {
    //input is in the following order:
    // TL ML BL TR MR BR
    msglock.lock(); //lock mutex protecting msg
    if (len >= 140) {   //if string is at maximum size
        msglock.unlock();   //unlock msg mutex
        //vibrate motor for a long time to denote an error
        braillevibrate = 1;
        ThisThread::sleep_for(500ms);
        braillevibrate = 0;
        return;
    }
    //number characters
    if (len > 0 && msg[len-1] == '#'){    //if number key was just input
        inputlock.lock();   //lock input mutex
        switch (input){
            case 0b000000: msg[len-1] = ' '; msg[len] = '#'; len ++; break; // blank = space
            case 0b100000: msg[len-1] = '1'; msg[len] = '#'; len ++; break; // ⠁ = 1
            case 0b110000: msg[len-1] = '2'; msg[len] = '#'; len ++; break; // ⠃ = 2
            case 0b100100: msg[len-1] = '3'; msg[len] = '#'; len ++; break; // ⠉ = 3
            case 0b100110: msg[len-1] = '4'; msg[len] = '#'; len ++; break; // ⠙ = 4
            case 0b100010: msg[len-1] = '5'; msg[len] = '#'; len ++; break; // ⠑ = 5
            case 0b110100: msg[len-1] = '6'; msg[len] = '#'; len ++; break; // ⠋ = 6
            case 0b110110: msg[len-1] = '7'; msg[len] = '#'; len ++; break; // ⠛ = 7
            case 0b110010: msg[len-1] = '8'; msg[len] = '#'; len ++; break; // ⠓ = 8
            case 0b010100: msg[len-1] = '9'; msg[len] = '#'; len ++; break; // ⠊ = 9
            case 0b010110: msg[len-1] = '0'; msg[len] = '#'; len ++; break; // ⠚ = 0
            case 0b000011: len--; msg[len] = '\0'; break; // ⠰ = switch back to letters by removing '#'
            default:    //invalid input
                msglock.unlock(); //unlock msg mutex
                inputlock.unlock(); //unlock input mutex
                //vibrate motor for a long time to denote an error
                braillevibrate = 1;
                ThisThread::sleep_for(500ms);
                braillevibrate = 0;
                return;
        }
        inputlock.unlock(); //unlock input mutex

    }
    //non-number characters
    else {
        inputlock.lock();   //lock input mutex
        switch (input){
            case 0b000000: msg[len] = ' '; len++; break; // blank space
            case 0b100000: msg[len] = 'a'; len++; break; // ⠁ = a
            case 0b110000: msg[len] = 'b'; len++; break; // ⠃ = b
            case 0b100100: msg[len] = 'c'; len++; break; // ⠉ = c
            case 0b100110: msg[len] = 'd'; len++; break; // ⠙ = d
            case 0b100010: msg[len] = 'e'; len++; break; // ⠑ = e
            case 0b110100: msg[len] = 'f'; len++; break; // ⠋ = f
            case 0b110110: msg[len] = 'g'; len++; break; // ⠛ = g
            case 0b110010: msg[len] = 'h'; len++; break; // ⠓ = h
            case 0b010100: msg[len] = 'i'; len++; break; // ⠊ = i
            case 0b010110: msg[len] = 'j'; len++; break; // ⠚ = j
            case 0b101000: msg[len] = 'k'; len++; break; // ⠅ = k
            case 0b111000: msg[len] = 'l'; len++; break; // ⠇ = l
            case 0b101100: msg[len] = 'm'; len++; break; // ⠍ = m
            case 0b101110: msg[len] = 'n'; len++; break; // ⠝ = n
            case 0b101010: msg[len] = 'o'; len++; break; // ⠕ = o
            case 0b111100: msg[len] = 'p'; len++; break; // ⠏ = p
            case 0b111110: msg[len] = 'q'; len++; break; // ⠟ = q
            case 0b111010: msg[len] = 'r'; len++; break; // ⠗ = r
            case 0b011100: msg[len] = 's'; len++; break; // ⠎ = s
            case 0b011110: msg[len] = 't'; len++; break; // ⠞ = t
            case 0b101001: msg[len] = 'u'; len++; break; // ⠥ = u
            case 0b111001: msg[len] = 'v'; len++; break; // ⠧ = v
            case 0b010111: msg[len] = 'w'; len++; break; // ⠺ = w
            case 0b101101: msg[len] = 'x'; len++; break; // ⠭ = x
            case 0b101111: msg[len] = 'y'; len++; break; // ⠽ = y
            case 0b101011: msg[len] = 'z'; len++; break; // ⠵ = z
            case 0b001111: msg[len] = '#'; len++; break; // ⠼ = switch to numbers by adding a '#'
            default: // invalid input
                msglock.unlock(); //unlock mutex protecting msg
                inputlock.unlock(); //unlock input mutex
                //vibrate motor for a long time to denote an error
                braillevibrate = 1;
                ThisThread::sleep_for(500ms);
                braillevibrate = 0;
                return;
        }
        inputlock.unlock(); //unlock input mutex
    }
    // character has been added to msg
    msglock.unlock(); //unlock mutex protecting msg
    //vibrate motor for a short time to denote successful character entry
    braillevibrate = 1;
    ThisThread::sleep_for(100ms);
    braillevibrate = 0;
    return;
}

//dot(): outputs a morse code "dot" to the vibration motor and LED
void dot() {
    morsevibrate = 1;   //turn on motor
    GPIOB->ODR |= 2;    //turn on led
    ThisThread::sleep_for(100ms);   //dots last for 1 unit (100ms)
    morsevibrate = 0;   //turn off motor
    GPIOB->ODR &= ~2;   //turn off led
    ThisThread::sleep_for(100ms);   //pauses between parts of a letter are one unit (100ms)
}
//dash(): outputs a morse code "dash" to the vibration motor and LED
void dash() {
    morsevibrate = 1;   //turn on motor
    GPIOB->ODR |= 2;    //turn on led
    ThisThread::sleep_for(300ms);   //dashes last for 3 units (300ms)
    morsevibrate = 0;   //turn off motor
    GPIOB->ODR &= ~2;   //turn off led
    ThisThread::sleep_for(100ms);   //pauses between parts of a letter are one unit (100ms)
}

//printMorse(): reads the msg string and outputs it as morse code via the vibration motor and LED
void printMorse() {
    msglock.lock(); //lock msg mutex
    char msgcpy[140] = {};  //create string to copy msg
    //copy msg into msgcopy and clear msg
    for (int i = 0; i < len && msg[i] != '\0'; i++) {msgcpy[i] = msg[i]; msg[i] = '\0';}    //copy msg into msgcopy; clear msg
    int lencpy = len;   //create copy of len
    len = 0;    //set len to 0
    msglock.unlock();   //unlock msg mutex; msg was copied to free this up sooner

    //loop through each character in msgcopy
    for (int i = 0; i < lencpy && msgcpy[i] != '\0'; i++) {
        switch (msgcpy[i]) {   //look at the character
            case ' ': ThisThread::sleep_for(200ms); break;//spaces between words take 7 units of time total (1 unit = 100ms)
            case 'a': dot(); dash();                    break; //a: .-
            case 'b': dash(); dot(); dot(); dot();      break; //b: -...
            case 'c': dash(); dot(); dash(); dot();     break; //c: -.-.
            case 'd': dash(); dot(); dot();             break; //d: -..
            case 'e': dot();                            break; //e: .
            case 'f': dot(); dot(); dash(); dot();      break; //f: ..-.
            case 'g': dash(); dash(); dot();            break; //g: --.
            case 'h': dot(); dot(); dot(); dot();       break; //h: ....
            case 'i': dot(); dot();                     break; //i: ..
            case 'j': dot(); dash(); dash(); dash();    break; //j: .---
            case 'k': dash(); dot(); dash();            break; //k: -.-
            case 'l': dot(); dash(); dot(); dot();      break; //l: .-..
            case 'm': dash(); dash();                   break; //m: --
            case 'n': dash(); dot();                    break; //n: -.
            case 'o': dash(); dash(); dash();           break; //o: ---
            case 'p': dot(); dash(); dash(); dot();     break; //p: .--.
            case 'q': dash(); dash(); dot(); dash();    break; //q: --.-
            case 'r': dot(); dash(); dot();             break; //r: .-.
            case 's': dot(); dot(); dot();              break; //s: ...
            case 't': dash();                           break; //t: -
            case 'u': dot(); dot(); dash();             break; //u: ..-
            case 'v': dot(); dot(); dot(); dash();      break; //v: ...-
            case 'w': dot(); dash(); dash();            break; //w: .--
            case 'x': dash(); dot(); dot(); dash();     break; //x: -..-
            case 'y': dash(); dot(); dash(); dash();    break; //y: -.--
            case 'z': dash(); dash(); dot(); dot();     break; //z: --..
            case '0': dash(); dash(); dash(); dash(); dash(); break;    //0: -----
            case '1': dot();  dash(); dash(); dash(); dash(); break;    //1: .----
            case '2': dot();  dot();  dash(); dash(); dash(); break;    //2: ..---
            case '3': dot();  dot();  dot();  dash(); dash(); break;    //3: ...--
            case '4': dot();  dot();  dot();  dot();  dash(); break;    //4: ....-
            case '5': dot();  dot();  dot();  dot();  dot();  break;    //5: .....
            case '6': dash(); dot();  dot();  dot();  dot();  break;    //6: -....
            case '7': dash(); dash(); dot();  dot();  dot();  break;    //7: --...
            case '8': dash(); dash(); dash(); dot();  dot();  break;    //8: ---..
            case '9': dash(); dash(); dash(); dash(); dot();  break;    //9: ----.
            case '#': break;    //ignore number marker if it was left in by user
        }
        ThisThread::sleep_for(200ms);   //sleep for 200ms to make a pause between letters 3 time units in total
    }
}