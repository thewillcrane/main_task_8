//=====[Libraries]=============================================================

#include "light_level_control.h"
#include "mbed.h"
#include "arm_book_lib.h"

#include "pc_serial_com.h"

#include "siren.h"
#include "fire_alarm.h"
#include "code.h"
#include "date_and_time.h"
#include "temperature_sensor.h"
#include "gas_sensor.h"
#include "event_log.h"
#include "motor.h"
#include "gate.h"
#include "motion_sensor.h"
#include "alarm.h"
#include "sd_card.h"

#include "display.h"
#include "user_interface.h"
#include "matrix_keypad.h"
#include <cstdio>

//=====[Declaration of private defines]========================================

#define MATRIX_KEYPAD_NUMBER_OF_ROWS    4
#define MATRIX_KEYPAD_NUMBER_OF_COLS    4

#define MAX_RECENT_EVENTS 10 //Part 4

//=====[Declaration of private data types]=====================================

typedef enum{
    PC_SERIAL_COMMANDS,
    PC_SERIAL_GET_CODE,
    PC_SERIAL_SAVE_NEW_CODE,
} pcSerialComMode_t;

//=====[Declaration and initialization of public global objects]===============

UnbufferedSerial uartUsb(USBTX, USBRX, 115200);

DigitalOut KeypadRowPins[MATRIX_KEYPAD_NUMBER_OF_ROWS] = {PB_3, PB_5, PC_7, PA_15};
DigitalIn KeypadColPins[MATRIX_KEYPAD_NUMBER_OF_COLS]  = {PB_12, PB_13, PB_15, PC_6};

//=====[Declaration of external public global variables]=======================

bool ePressed = false; //Check if event has been logged

//=====[Declaration and initialization of public global variables]=============

char codeSequenceFromPcSerialCom[CODE_NUMBER_OF_KEYS];

//=====[Declaration and initialization of private global variables]============

static pcSerialComMode_t pcSerialComMode = PC_SERIAL_COMMANDS;
static bool codeComplete = false;
static int numberOfCodeChars = 0;



//=====[Declarations (prototypes) of private functions]========================

static void pcSerialComStringRead( char* str, int strLength );

static void pcSerialComGetCodeUpdate( char receivedChar );
static void pcSerialComSaveNewCodeUpdate( char receivedChar );

static void pcSerialComCommandUpdate( char receivedChar );

static void availableCommands();
static void commandShowCurrentAlarmState();
static void commandShowCurrentGasDetectorState();
static void commandShowCurrentOverTemperatureDetectorState();
static void commandEnterCodeSequence();
static void commandEnterNewCode();
static void commandShowCurrentTemperatureInCelsius();
static void commandShowCurrentTemperatureInFahrenheit();
static void commandSetDateAndTime();
static void commandShowDateAndTime();
static void commandShowStoredEvents();
static void commandShowCurrentMotorState();
static void commandShowCurrentGateState();
static void commandMotionSensorActivate();
static void commandMotionSensorDeactivate();
static void commandEventLogSaveToSdCard();

static char MatrixKeypadScan(); //Part 3

static void readEventsFromSDCard();//Part 4

static void commandShowPotentiometer();

//=====[Implementations of public functions]===================================

void pcSerialComInit()
{
    availableCommands();
}

char pcSerialComCharRead()
{
    char receivedChar = '\0';
    if( uartUsb.readable() ) {
        uartUsb.read( &receivedChar, 1 );
    }
    return receivedChar;
}

void pcSerialComStringWrite( const char* str )
{
    uartUsb.write( str, strlen(str) );
}

void pcSerialComIntWrite( int number )
{
    char str[4] = "";
    sprintf( str, "%d", number );
    pcSerialComStringWrite( str );
}

void pcSerialComUpdate()
{
    char receivedChar = pcSerialComCharRead();
    if( receivedChar != '\0' ) {
        switch ( pcSerialComMode ) {
            case PC_SERIAL_COMMANDS:
                pcSerialComCommandUpdate( receivedChar );
            break;

            case PC_SERIAL_GET_CODE:
                pcSerialComGetCodeUpdate( receivedChar );
            break;

            case PC_SERIAL_SAVE_NEW_CODE:
                pcSerialComSaveNewCodeUpdate( receivedChar );
            break;
            default:
                pcSerialComMode = PC_SERIAL_COMMANDS;
            break;
        }
    }    
}

bool pcSerialComCodeCompleteRead()
{
    return codeComplete;
}

void pcSerialComCodeCompleteWrite( bool state )
{
    codeComplete = state;
}

bool eventLogged(){
    return ( ePressed );
}

void LCDDisplay();

//=====[Implementations of private functions]==================================

static void pcSerialComStringRead( char* str, int strLength )
{
    int strIndex;
    for ( strIndex = 0; strIndex < strLength; strIndex++) {
        uartUsb.read( &str[strIndex] , 1 );
        uartUsb.write( &str[strIndex] ,1 );
    }
    str[strLength]='\0';
}

static void pcSerialComGetCodeUpdate( char receivedChar )
{
    codeSequenceFromPcSerialCom[numberOfCodeChars] = receivedChar;
    pcSerialComStringWrite( "*" );
    numberOfCodeChars++;
   if ( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
        pcSerialComMode = PC_SERIAL_COMMANDS;
        codeComplete = true;
        numberOfCodeChars = 0;
    } 
}

static void pcSerialComSaveNewCodeUpdate( char receivedChar )
{
    static char newCodeSequence[CODE_NUMBER_OF_KEYS];

    newCodeSequence[numberOfCodeChars] = receivedChar;
    pcSerialComStringWrite( "*" );
    numberOfCodeChars++;
    if ( numberOfCodeChars >= CODE_NUMBER_OF_KEYS ) {
        pcSerialComMode = PC_SERIAL_COMMANDS;
        numberOfCodeChars = 0;
        codeWrite( newCodeSequence );
        pcSerialComStringWrite( "\r\nNew code configured\r\n\r\n" );
    } 
}

static void pcSerialComCommandUpdate( char receivedChar )
{
    switch (receivedChar) {
        case '1': commandShowCurrentAlarmState(); break;
        case '2': commandShowCurrentGasDetectorState(); break;
        case '3': commandShowCurrentOverTemperatureDetectorState(); break;
        case '4': commandEnterCodeSequence(); break;
        case '5': commandEnterNewCode(); break;
        case 'c': case 'C': commandShowCurrentTemperatureInCelsius(); break;
        case 'f': case 'F': commandShowCurrentTemperatureInFahrenheit(); break;
        case 's': case 'S': commandSetDateAndTime(); break;
        case 't': case 'T': commandShowDateAndTime(); break;
        case 'e': case 'E': commandShowStoredEvents(); break;
        case 'm': case 'M': commandShowCurrentMotorState(); break;
        case 'g': case 'G': commandShowCurrentGateState(); break;
        case 'i': case 'I': commandMotionSensorActivate(); break;
        case 'h': case 'H': commandMotionSensorDeactivate(); break;
        case 'w': case 'W': commandEventLogSaveToSdCard(); break;
        case 'p': case 'P': commandShowPotentiometer(); break; //Part 5
        default: availableCommands(); break;
    } 
}

static void readEventsFromSDCard()
{
    // Find the most recent file
    char filename[SD_CARD_FILENAME_MAX_LENGTH] = "";
    time_t latestTime = 0;
    DIR *dir = opendir("/sd/");
    if (dir == NULL) {
        pcSerialComStringWrite("Error: Could not open SD card directory /fs/.\r\n");
        displayCharPositionWrite(0, 0);
        displayStringWrite("SD Card Error       ");
        displayCharPositionWrite(0, 1);
        displayStringWrite("Cannot open dir     ");
        ThisThread::sleep_for(3s);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strlen(entry->d_name) == 23 && strstr(entry->d_name, ".txt")) {
            struct tm fileTime = {0};
            if (sscanf(entry->d_name, "%4d_%2d_%2d_%2d_%2d_%2d.txt",
                       &fileTime.tm_year, &fileTime.tm_mon, &fileTime.tm_mday,
                       &fileTime.tm_hour, &fileTime.tm_min, &fileTime.tm_sec) == 6) {
                fileTime.tm_year -= 1900;
                fileTime.tm_mon -= 1;
                fileTime.tm_isdst = -1;
                time_t fileSeconds = mktime(&fileTime);
                if (fileSeconds != -1 && fileSeconds > latestTime) {
                    latestTime = fileSeconds;
                    strcpy(filename, entry->d_name);
                }
            }
        }
    }
    closedir(dir);

    if (filename[0] == '\0') {
        pcSerialComStringWrite("No event files found on SD card.\r\n");
        displayCharPositionWrite(0, 0);
        displayStringWrite("No event files      ");
        displayCharPositionWrite(0, 1);
        displayStringWrite("Check SD card       ");
        ThisThread::sleep_for(3s);
        return;
    }

    // Open the file
    char filePath[SD_CARD_FILENAME_MAX_LENGTH + 5];
    sprintf(filePath, "/sd/%s", filename);
    FILE *file = fopen(filePath, "r");
    if (file == NULL) {
        pcSerialComStringWrite("Error: Could not open ");
        pcSerialComStringWrite(filename);
        pcSerialComStringWrite("\r\n");
        displayCharPositionWrite(0, 0);
        displayStringWrite("SD Card Error       ");
        displayCharPositionWrite(0, 1);
        displayStringWrite("Cannot open file    ");
        ThisThread::sleep_for(3s);
        return;
    }

    // Display reading message
    pcSerialComStringWrite("Reading recent events from ");
    pcSerialComStringWrite(filename);
    pcSerialComStringWrite(":\r\n");
    displayCharPositionWrite(0, 0);
    displayStringWrite("Reading SD card...  ");
    displayCharPositionWrite(0, 1);
    displayStringWrite("                    ");

    // Read and output file contents line by line
    char buffer[EVENT_STR_LENGTH];
    while (fgets(buffer, EVENT_STR_LENGTH, file) != NULL) {
        pcSerialComStringWrite(buffer);
        //Part 5
        if(lightLevelControlRead() < 0.5){
            ThisThread::sleep_for(3s); //Slow scrolling when potentimeter is low
        }else if(lightLevelControlRead() >= 0.5){
            ThisThread::sleep_for(1s);  //Fast scrolling when potentiometer is high
        }
    }

    // Close the file
    fclose(file);

    // Display completion message
    pcSerialComStringWrite("Finished reading recent events from ");
    pcSerialComStringWrite(filename);
    pcSerialComStringWrite("\r\n");
    displayCharPositionWrite(0, 0);
    displayStringWrite("SD card read done   ");
    displayCharPositionWrite(0, 1);
    displayStringWrite("Check serial monitor");
    ThisThread::sleep_for(3s);
}

static void availableCommands()
{
    pcSerialComStringWrite( "Available commands:\r\n" );
    pcSerialComStringWrite( "Press '1' to get the alarm state\r\n" );
    pcSerialComStringWrite( "Press '2' to get the gas detector state\r\n" );
    pcSerialComStringWrite( "Press '3' to get the over temperature detector state\r\n" );
    pcSerialComStringWrite( "Press '4' to enter the code to deactivate the alarm\r\n" );
    pcSerialComStringWrite( "Press '5' to enter a new code to deactivate the alarm\r\n" );
    pcSerialComStringWrite( "Press 'f' or 'F' to get lm35 reading in Fahrenheit\r\n" );
    pcSerialComStringWrite( "Press 'c' or 'C' to get lm35 reading in Celsius\r\n" );
    pcSerialComStringWrite( "Press 's' or 'S' to set the date and time\r\n" );
    pcSerialComStringWrite( "Press 't' or 'T' to get the date and time\r\n" );
    pcSerialComStringWrite( "Press 'e' or 'E' to get the stored events\r\n" );
    pcSerialComStringWrite( "Press 'm' or 'M' to show the motor status\r\n" );
    pcSerialComStringWrite( "Press 'g' or 'G' to show the gate status\r\n" );    
    pcSerialComStringWrite( "Press 'i' or 'I' to activate the motion sensor\r\n" );
    pcSerialComStringWrite( "Press 'h' or 'H' to deactivate the motion sensor\r\n" );
    pcSerialComStringWrite( "Press 'w' or 'W' to store the events log in the SD card\r\n" );
    pcSerialComStringWrite( "Press 'p' or 'P' to show potentiometer reading\r\n" );
    pcSerialComStringWrite( "\r\n" );
}

static void commandShowCurrentAlarmState()
{
    if ( alarmStateRead() ) {
        pcSerialComStringWrite( "The alarm is activated\r\n");
    } else {
        pcSerialComStringWrite( "The alarm is not activated\r\n");
    }
}

static void commandShowCurrentGasDetectorState()
{
    if ( gasDetectorStateRead() ) {
        pcSerialComStringWrite( "Gas is being detected\r\n");
    } else {
        pcSerialComStringWrite( "Gas is not being detected\r\n");
    }    
}

static void commandShowCurrentOverTemperatureDetectorState()
{
    if ( overTemperatureDetectorStateRead() ) {
        pcSerialComStringWrite( "Temperature is above the maximum level\r\n");
    } else {
        pcSerialComStringWrite( "Temperature is below the maximum level\r\n");
    }
}

static void commandEnterCodeSequence()
{
    if( alarmStateRead() ) {
        pcSerialComStringWrite( "Please enter the four digits numeric code " );
        pcSerialComStringWrite( "to deactivate the alarm: " );
        pcSerialComMode = PC_SERIAL_GET_CODE;
        codeComplete = false;
        numberOfCodeChars = 0;
    } else {
        pcSerialComStringWrite( "Alarm is not activated.\r\n" );
    }
}

static void commandEnterNewCode()
{
    pcSerialComStringWrite( "Please enter the new four digits numeric code " );
    pcSerialComStringWrite( "to deactivate the alarm: " );
    numberOfCodeChars = 0;
    pcSerialComMode = PC_SERIAL_SAVE_NEW_CODE;

}

static void commandShowCurrentTemperatureInCelsius()
{
    char str[100] = "";
    sprintf ( str, "Temperature: %.2f \xB0 C\r\n",
                    temperatureSensorReadCelsius() );
    pcSerialComStringWrite( str );  
}

static void commandShowCurrentTemperatureInFahrenheit()
{
    char str[100] = "";
    sprintf ( str, "Temperature: %.2f \xB0 C\r\n",
                    temperatureSensorReadFahrenheit() );
    pcSerialComStringWrite( str );  
}

static void commandEventLogSaveToSdCard()
{
    eventLogSaveToSdCard();
}

static void commandSetDateAndTime()
{
     

    struct tm rtcTime = {0}; // Initialize to zero
    char inputBuffer[5];     // Buffer for 4 digits + null terminator
    int strIndex;

            // Year
            uartUsb.write( "\r\nType four digits for the current year (YYYY): ", 48 );
            for( strIndex = 0; strIndex < 4; strIndex++ ) {
                while( !uartUsb.readable() ); // Wait for input
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[4] = '\0'; // Null terminate
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_year = atoi(inputBuffer) - 1900;

            // Month
            uartUsb.write( "Type two digits for the current month (01-12): ", 47 );
            for( strIndex = 0; strIndex < 2; strIndex++ ) {
                while( !uartUsb.readable() );
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[2] = '\0';
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_mon = atoi(inputBuffer) - 1;

            // Day
            uartUsb.write( "Type two digits for the current day (01-31): ", 45 );
            for( strIndex = 0; strIndex < 2; strIndex++ ) {
                while( !uartUsb.readable() );
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[2] = '\0';
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_mday = atoi(inputBuffer);

            // Hour
            uartUsb.write( "Type two digits for the current hour (00-23): ", 46 );
            for( strIndex = 0; strIndex < 2; strIndex++ ) {
                while( !uartUsb.readable() );
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[2] = '\0';
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_hour = atoi(inputBuffer);

            // Minutes
            uartUsb.write( "Type two digits for the current minutes (00-59): ", 49 );
            for( strIndex = 0; strIndex < 2; strIndex++ ) {
                while( !uartUsb.readable() );
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[2] = '\0';
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_min = atoi(inputBuffer);

            // Seconds
            uartUsb.write( "Type two digits for the current seconds (00-59): ", 49 );
            for( strIndex = 0; strIndex < 2; strIndex++ ) {
                while( !uartUsb.readable() );
                uartUsb.read( &inputBuffer[strIndex], 1 );
            }
            inputBuffer[2] = '\0';
            uartUsb.write( "\r\n", 2 );
            rtcTime.tm_sec = atoi(inputBuffer);

            // Set the time
            rtcTime.tm_isdst = -1; 
            set_time( mktime(&rtcTime) );
            uartUsb.write( "Date and time has been set\r\n", 28 );                              
}

static void commandShowDateAndTime()
{
    char str[100] = "";
    sprintf ( str, "Date and Time = %s", dateAndTimeRead() );
    pcSerialComStringWrite( str );
    pcSerialComStringWrite("\r\n");
}

static void commandShowStoredEvents()
{
    ePressed = true;
    char str[EVENT_STR_LENGTH] = "";
    int i;

    int totalEvents = eventLogNumberOfStoredEvents();

    for (i = 0; i < eventLogNumberOfStoredEvents(); i++) {
        eventLogRead( i, str );
        pcSerialComStringWrite( str );   
        pcSerialComStringWrite( "\r\n" );                    
    }

    //Confirmation message on the serial monitor
    pcSerialComStringWrite("YOUR EVENTS HAVE BEEN LOGGED SUCCESSFULLY!");
    pcSerialComStringWrite("\r\n");

    //Confirmation message on LCD
    displayCharPositionWrite(0, 0);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 1);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 2);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 3);
            displayStringWrite("                    ");

            displayCharPositionWrite(0, 0);
            displayStringWrite("Event has been      ");
            displayCharPositionWrite(0, 1);
            displayStringWrite("logged successfully!");
            ThisThread::sleep_for(3s); //Turn off at 3 seconds

            displayCharPositionWrite(0, 0);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 1);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 2);
            displayStringWrite("                    ");
            displayCharPositionWrite(0, 3);
            displayStringWrite("                    ");        
    
    //Part 3
    //Prompt to select specific event through serial monitor
    pcSerialComStringWrite("Please select what event ypu would like to access.\n");
    pcSerialComStringWrite("Do this by selecting the number on the matrix keypad minus 1.");
    pcSerialComStringWrite("\r\n");
    //Part 4
    pcSerialComStringWrite("OR select A for recent events from SD card.\n\n");

    //Prompt to select specific event on LCD
    displayCharPositionWrite(0, 0);
    displayStringWrite("Please select event:");
    displayCharPositionWrite(0, 1);
    displayStringWrite("By pressing the same");
    displayCharPositionWrite(0, 2);
    displayStringWrite("number on kepyad - 1");
    //Part 4
    displayCharPositionWrite(0, 3);
    displayStringWrite("OR A for SD events..");

    //Wait for keypad input
    char key = '\0';
    while (key == '\0') {
        key = MatrixKeypadScan(); // Assumed function from user_interface.h
        ThisThread::sleep_for(100ms);
    }

    //Part 4
    if (key == 'A'){
        displayCharPositionWrite(0, 0);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 1);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 2);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 3);
        displayStringWrite("                    ");
        // Read events from SD card
        readEventsFromSDCard();
    }else{
    //Part 3 again
    int selectedEvent = key - '0'; //Convert char to int
    if (key >= '0' && key<= '9' && selectedEvent >= 0 && selectedEvent < totalEvents){
        //Select selected event
        eventLogRead(selectedEvent, str);

        //Display this on serial monitor
        pcSerialComStringWrite("Selected Event ");
        pcSerialComStringWrite(": ");
        pcSerialComStringWrite(str);
        pcSerialComStringWrite("\r\n");

        //Display on LCD
        displayCharPositionWrite(0, 0);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 1);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 2);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 3);
        displayStringWrite("                    ");
        displayCharPositionWrite(0, 0);
        //Check if string is over 20 so it moves onto new line if does
        char line1[21] = "";
        char line2[21] = "";
        char line3[21] = "";
        char line4[21] = "";
        strncpy(line1, str, 20);
        line1[20] = '\0';
        if (strlen(str) > 20) {
            strncpy(line2, str + 20, 20);
            line2[20] = '\0';
            if (strlen(str) > 40) {
                strncpy(line3, str + 40, 20);
                line3[20] = '\0';
                if (strlen(str) > 60) {
                    strncpy(line4, str + 60, 20);
                    line4[20] = '\0';
                }
            }
        }
        displayStringWrite(line1);
        displayCharPositionWrite(0, 1);
        displayStringWrite(line2);
        displayCharPositionWrite(0, 2);
        displayStringWrite(line3);
        displayCharPositionWrite(0, 3);
        displayStringWrite(line4);
        ThisThread::sleep_for(5s); //Display for 5 seconds
    }else{
        //Invalid input on serial monitor
        pcSerialComStringWrite("Invalid event number. Please select a number between 0 and ");
        pcSerialComIntWrite(totalEvents - 1);
        pcSerialComStringWrite(".\r\n");

        //Invalid display on LCD
        displayCharPositionWrite(0, 0);
        displayStringWrite("INVALID EVENT NUMBER");
        displayCharPositionWrite(0, 1);
        displayStringWrite("TRY AGAIN!          ");
        ThisThread::sleep_for(3s); // Display for 3 seconds
    }
    }

    //Clear LCD after completion
    displayCharPositionWrite(0, 0);
    displayStringWrite("                    ");
    displayCharPositionWrite(0, 1);
    displayStringWrite("                    ");
    displayCharPositionWrite(0, 2);
    displayStringWrite("                    ");
    displayCharPositionWrite(0, 3);
    displayStringWrite("                    ");


    ePressed = false;    
}

static void commandShowCurrentMotorState()
{
    switch ( motorDirectionRead() ) {
        case STOPPED: 
            pcSerialComStringWrite( "The motor is stopped\r\n"); break;
        case DIRECTION_1: 
            pcSerialComStringWrite( "The motor is turning in direction 1\r\n"); break;
        case DIRECTION_2: 
            pcSerialComStringWrite( "The motor is turning in direction 2\r\n"); break;
    }
}

static void commandShowCurrentGateState()
{
    switch ( gateStatusRead() ) {
        case GATE_CLOSED: pcSerialComStringWrite( "The gate is closed\r\n"); break;
        case GATE_OPEN: pcSerialComStringWrite( "The gate is open\r\n"); break;
        case GATE_OPENING: pcSerialComStringWrite( "The gate is opening\r\n"); break;
        case GATE_CLOSING: pcSerialComStringWrite( "The gate is closing\r\n"); break;
    }
}

static void commandMotionSensorActivate()
{
    motionSensorActivate();
}

static void commandMotionSensorDeactivate()
{
    motionSensorDeactivate();
}

static char MatrixKeypadScan()
{
    int row = 0;
    int col = 0;
    int i = 0; 

    char matrixKeypadIndexToCharArray[] = {
        '1', '2', '3', 'A',
        '4', '5', '6', 'B',
        '7', '8', '9', 'C',
        '*', '0', '#', 'D',
    };

    for( row=0; row<MATRIX_KEYPAD_NUMBER_OF_ROWS; row++ ) {

        for( i=0; i<MATRIX_KEYPAD_NUMBER_OF_ROWS; i++ ) {
            KeypadRowPins[i] = ON;
        }

        KeypadRowPins[row] = OFF;

        for( col=0; col<MATRIX_KEYPAD_NUMBER_OF_COLS; col++ ) {
            if( KeypadColPins[col] == OFF ) {
                return matrixKeypadIndexToCharArray[
                    row*MATRIX_KEYPAD_NUMBER_OF_ROWS + col];
            }
        }
    }
    return '\0';
}

static void commandShowPotentiometer(){
    float potentiometer = lightLevelControlRead(); //Read the potentiometer value (0.0 to 1.0)
    char str[100];
    sprintf(str, "Potentiometer reading: %.2f\r\n", potentiometer); //Format the value
    pcSerialComStringWrite(str);
}
