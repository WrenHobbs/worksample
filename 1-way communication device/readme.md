-------------------
About
-------------------
Project Description:
    This system is a one-way communication device that allows one user to send remote tactile messages to another.
    The system does not require vision or hearing for either party to use it effectively.
		
Contribitor List:
	Wren Hobbs

--------------------
Features
--------------------
    - Six-bump braille keyboard with tactile user feedback
    - Alphanumeric message entry up to 140 characters long
    - Morse code output via vibration motor and LED
    - Functionality for concurrent user input and output
    - System protection via watchdog
    - Mutex-based critical section protection


--------------------
Required Materials
--------------------
    - Nucleo L4R5ZI
    - 9x push buttons
    - 2x vibration motors
    - 1x LED
    - 1x 1kΩ resistor

--------------------
Getting Started
--------------------
    - Connect one end of all nine push buttons to PD 0 (vcc)
        For the other end of each push button:
        - Top-left bump button: connect to PC 6
        - Middle-left bump button: connect to PC 8
        - Bottom-left bump button: connect to PC 9
        - Top-right bump button: connect to PC 10
        - Middle-right bump button: connect to PC 11
        - Bottom-right bump button: connect to PC 12
        - Enter button: connect to PB 15
        - Backspace button: connect to PB 13
        - Send button: connect to PB 12
    - Connect the resistor and LED in series; connect the positive end to PB 1 and the negative end to ground.
    - Connect one end of both vibration motors to ground.
    - Connect the other end of the morse vibration motor to PF 8.
    - Connect the other end of the braille keyboard vibration motor to PE 14.


----------
Declarations
----------

Mutexes:
    msglock: protects the msg and len global variables
    inputlock: protects the input global variable
    enterlock: used by enterThread and enterISR to protect against bounce
    backspacelock: used by backspaceThread and backspaceISR to protect against bounce
    sendlock: used by sendThread and sendISR to protect against bounce

Condition Variables:
    enterCond: enterThread will wait on this, and enterISR will signal
    backspaceCond: backspaceThread will wait on this, and backspaceISR will signal
    sendCond: sendThread will wait on this, and sendISR will signal

Interrupts:
    enterButton: calls enterISR
    backspaceButton: calls backspaceISR
    sendButton: calls sendISR

Bus:
    bumps: holds the 6 bump button inputs

Outputs:
    braillevibrate: vibration motor attached to braille keyboard to give user tactile feedback
    morsevibrate: vibration motor that is used to output the message in morse code
    + bitwise output used for the morse LED

Threads:
    isrThread: handles the event queue that the ISRs go through
    enterThread: handles functionality of enter button
    backspaceThread: handles functionality of backspace button
    sendThread: handles functionality of send button

EventQueue:
    queue: used by ISRs

ISR functions:
    enterISR: called when enterButton is pressed, signals enterThread
    backspaceISR: called when backspaceButton is pressed, signals backspaceThread
    sendISR: called when sendButton is pressed, signals sendThread

Thread functions:
    enter: used by enterThread
    backspace: used by backspaceThread
    send: used by sendThread

Global Variables:
    msg: character array of size 140; stores the user's inputted message
    len: stores the length of the message
    input: stores the bump button bus state as a 6-bit number, each bit corresponding to one button

----------
API and Built In Elements Used
----------

Thread
EventQueue
Mutex
ConditionVariable
Bus
Watchdog

----------
Custom Functions
----------
    brailleToText: takes a 6-bit number (i.e. 0x111000) and interprets that as a braille character,
    each bit corresponding to one of the bumps. Returns the character that set of bumps is associated with.

    printMorse: reads the message and outputs the morse code representation.

    dot: used by printMorse; outputs a morse "dot" signal.

    dash: used by printMorse; outputs a morse "dash" signal.