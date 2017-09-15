/*
 ******************************************************************************
  Copyright (c) 2015 Particle Industries, Inc.  All rights reserved.

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation, either
  version 3 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.
  ******************************************************************************
 */

/* Includes ------------------------------------------------------------------*/
#include "application.h"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(2);

// ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, INFO_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL
SerialDebugOutput debugOutput(115200, ALL_LEVEL);

STARTUP(cellular_credentials_set("pda.bell.ca", "", "", NULL)); // BELL
//STARTUP(cellular_credentials_set("broadband", "", "", NULL)); // AT&T

/* Function prototypes -------------------------------------------------------*/
int tinkerDigitalRead(String pin);
int tinkerDigitalWrite(String command);
int tinkerAnalogRead(String pin);
int tinkerAnalogWrite(String command);

//SYSTEM_MODE(AUTOMATIC);
//SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_MODE(MANUAL);

uint32_t lastUpdate = 0;
bool D7state = false;

typedef struct { char* buf; char* num; } CMGRparam;
//static int _cbCUSD(int type, const char* buf, int len, char* resp);
typedef struct { int* ix; int num; } CMGLparam;
int _cbCMGL(int type, const char* buf, int len, CMGLparam* param);
int _cbCMGR(int type, const char* buf, int len, CMGRparam* param);
int smsList(const char* stat /*= "ALL"*/, int* ix /*=NULL*/, int num /*= 0*/);
bool smsSend(const char* num, const char* buf);
bool smsDelete(int ix);
bool smsRead(int ix, char* num, char* buf, int len);
void checkUnreadSMS();
void checkReadSMS();
void processATcommand();
void showHelp();

int _cbCMGL(int type, const char* buf, int len, CMGLparam* param)
{
    if ((type == TYPE_PLUS) && param && param->num) {
        // +CMGL: <ix>,...
        int ix;
        if (sscanf(buf, "\r\n+CMGL: %d,", &ix) == 1)
        {
            *param->ix++ = ix;
            param->num--;
        }
    }
    return WAIT;
}

int smsList(const char* stat /*= "ALL"*/, int* ix /*=NULL*/, int num /*= 0*/) {
    int ret = -1;
    CMGLparam param;
    param.ix = ix;
    param.num = num;
    if (RESP_OK == Cellular.command(_cbCMGL, &param, "AT+CMGL=\"%s\"\r\n", stat))
        ret = num - param.num;
    return ret;
}

/*
bool smsSend(const char* num, const char* buf)
{
    //REF:
    //https://community.particle.io/t/sending-sms-from-particle-electron-using-at-commands/21020
    int resp;
    bool ok = false;
    char tmp[64];

    resp = Cellular.command(1000, "AT+CMGF=1\r\n"); //Start SMS
    Serial.printf("got (%d) in send 1nd command \r\n",resp);

    // numberformat 145=ISDN/ITU (uses +13061234567) 
    // vs numberformat 129 *seems* to be all else (ie: 3061234567)
    //~english - (<TIMEOUT>,<ATCMD>="<PHONENUM>",<NUM_FORMAT>)

    //sprintf(pnumAtcmd, "AT+CMGS
    resp = Cellular.command(1000, "AT+CMGS=\"%s\",145\r\n", num);
    Serial.printf("got (%d) in send 2nd command \r\n",resp);

    // Close with <MESSAGE>CTRL+Z (or 0x1A)
    // Note: no need for /r/n shiz in sms messages
    resp = Cellular.command(60*1000, "%s", buf);
    Serial.printf("got (%d) in send 3rd command \r\n",resp);

    resp = Cellular.command(60*1000, "%c", 0X1A);
    Serial.printf("got (%d) in send 4th command \r\n",resp);
    ok = (resp == RESP_OK); 
    return ok;
}
*/


//Commenting out the default and remaking my own
bool smsSend(const char* num, const char* buf)
{
    bool ok = false;
    int resp;
    //Serial.printf("num: %s buf: %s",num,buf);
    //resp = Cellular.command(1000, "AT+CMGF=1\r\n", num);
    //Serial.printf("got (%d) on CMGF CMD \r\n",resp);
    resp = Cellular.command(1000, "AT+CMGS=\"%s\",129\r\n", num);
    if (RESP_PROMPT == resp) {
        //Serial.printf("got RESP_PROMPT (%d) in send\r\n",resp);
        resp = Cellular.command(10, "%s", buf);
        //Serial.printf("got (%d) in send 2nd command \r\n",resp);
        resp = Cellular.command(60*1000, "%c", 0x1A);
        //Serial.printf("got (%d) in send 3nd command \r\n",resp);
        ok = (RESP_OK == resp); // ctrl-Z
    }
    return ok;
}


bool smsDelete(int ix)
{
    /*
    * AT+CMGD=<index>[,<flag>]
    *
    * <index> = Storage position
    *
    * <flag> = Deletion flag. If present, and different from 0, <index> is ignored:
    * • 0 (default value): delete the message specified in <index>
    * • 1: delete all the read messages from the preferred message storage, leaving unread messages
    * and stored mobile originated messages (whether sent or not) untouched
    * • 2: delete all the read messages from the preferred message storage and sent mobile originated
    * messages, leaving unread messages and unsent mobile originated messages untouched
    * • 3: delete all the read messages from the preferred message storage, sent and unsent mobile
    * originated messages leaving unread messages untouched
    * • 4: delete all the messages from the preferred message storage including unread messages
    */
    bool ok = false;
    ok = (RESP_OK == Cellular.command("AT+CMGD=%d\r\n", ix));
    return ok;
}

int _cbCMGR(int type, const char* buf, int len, CMGRparam* param)
{

    //Serial.printf("\r\nfirst cbCMGR: %s\r\n", buf);
    //Serial.printf("length: %d\r\n",len);
    //Serial.printf("Type: %d\r\n",type);
    
    if (type == TYPE_PLUS) {
        if (sscanf(buf, "\r\n+CMGR: \"%*[^\"]\",\"%[^\"]", param->num) == 1) {
            //buf="";//if it's the number, store it and clear the buffer
        }
    }
    else if ((type == TYPE_UNKNOWN) && (len>2)) {
        memcpy(param->buf, buf, len);
        param->buf[len] = '\0';
        //Serial.printf("\r\nNEW: after param->buf=%s\r\n",param->buf);
    } 
    //if (param) {
    if (0) {
        if (type == TYPE_PLUS) {
            if (sscanf(buf, "\r\n+CMGR: \"%*[^\"]\",\"%[^\"]", param->num) == 1) {
            }
        Serial.printf("\r\ncbCMGR: type=%d buf=%s len=%d DONE\r\n",type,buf,len);
        //Serial.printf("cbCMGR: type=%d buf[len-2]=%x buf[len-1]=%x  DONE\r\n",type,buf[len-2],buf[len-1]);
        //WILL: Changed this to match texts recieved from beths phone
        //Serial.printf("\r\ncbCMGR: param->buf=%s\r\n",param->buf);
        }if ((type == TYPE_PLUS) && (buf[len-2] == '\r') && (buf[len-1] == '\n')) {
            memcpy(param->buf, buf, len-2);
            param->buf[len-2] = '\0';
            Serial.printf("\r\ncbCMGR: after param->buf=%s\r\n",param->buf);
        }
    }
    return WAIT;
}

bool smsRead(int ix, char* num, char* buf, int len)
{
    bool ok = false;
    CMGRparam param;
    param.num = num;
    param.buf = buf;
    ok = (RESP_OK == Cellular.command(_cbCMGR, &param, "AT+CMGR=%d\r\n", ix));
    return ok;
}

void checkUnreadSMS() {
    // checking unread sms, looking for matching for D7 or special messages, send reply, then delete the message.
    char buf[512] = "";
    int ix[8];
    int n = smsList("REC UNREAD", ix, 8);
    bool sendStatus=0;
    if (n > 8) n = 8;
    while (n-- > 0)
    {
        char num[32];
        Serial.printf("Unread SMS at index %d\r\n", ix[n]);
        if (smsRead(ix[n], num, buf, sizeof(buf))) {
            Serial.printf("Got SMS from \"%s\" with text \"%s\"\r\n", num, buf);
            // provide a generic reply
            const char* reply = "Hi, Wills Sim card is in a test device this is an automated reply as your text is now lost";

            // If message contains D7 or d7
            if (strstr(buf, "led7") || strstr(buf, "Led7")) {
                if (getPinMode(D7) != OUTPUT) D7state = false; // assume D7 was not just an output, make sure the LED turns ON.
                D7state = !D7state; // toggle the D7 LED
                pinMode(D7, OUTPUT);
                digitalWrite(D7, D7state);
                if (D7state) reply = "D7 LED toggled on";
                else reply = "D7 LED toggled off";
            }

            // If the message contains "Who are you" it will reply with something specific
            // we don't check the W in case it's upper/lower case.
            if (strstr(buf, /*w*/"ho are you"))
                reply = "I am the automated AI in the SusText - written by Will!";

            Serial.printf("Send SMS reply \"%s\" to \"%s\"\r\n", reply, num);
            sendStatus=smsSend(num, reply);
            Serial.printf("Send function returned %d\r\n",sendStatus);

            // All done with this message, let's delete it
            Serial.printf("Delete SMS at index %d\r\n", ix[n]);
            smsDelete(ix[n]);
        }
    }
}

void checkReadSMS() {
    // checking read sms, and deleting
    char buf[512] = "";
    int ix[8];
    int n = smsList("REC READ", ix, 8);
    if (n > 8) n = 8;
    while (n-- > 0)
    {
        char num[32];
        Serial.printf("Read SMS at index %d\r\n", ix[n]);
        if (smsRead(ix[n], num, buf, sizeof(buf))) {
            Serial.printf("Got SMS from \"%s\" with text \"%s\"\r\n", num, buf);
            Serial.printf("Delete SMS at index %d\r\n", ix[n]);
            smsDelete(ix[n]);
        }
    }
}

/* This function is called once at start up ----------------------------------*/
void setup()
{
    int resp;
    pinMode(D7, OUTPUT);
    digitalWrite(D7, LOW);
    Serial.begin(115200);
    Cellular.on();
    digitalWrite(D7, HIGH);
    //delay(500); //Give it time to warm up ARBITRARY
    Cellular.connect();
    while (Cellular.connecting()){
        digitalWrite(D7, HIGH);
        //delay(100);
        digitalWrite(D7, LOW);
        //delay(100);
    }
    resp = Cellular.command(1000, "AT+CMGF=1\r\n");
    Serial.printf("Cellular connected, set mode to text: %d (-2==OK)\r\n",resp);

    //Setup the Tinker application here

    //Register all the Tinker functions
    //Particle.function("digitalread", tinkerDigitalRead);
    //Particle.function("digitalwrite", tinkerDigitalWrite);

    //Particle.function("analogread", tinkerAnalogRead);
    //Particle.function("analogwrite", tinkerAnalogWrite);
}

/* This function loops forever --------------------------------------------*/
void loop()
{
    
    // Check for new SMS every 1 second, process if one is received.
    if (millis() - lastUpdate > 1000UL) {
        lastUpdate = millis();
        Serial.printf("1 second CHECK\r\n");
        checkUnreadSMS();
    }

    if (Serial.available() > 0)
    {
        char c = Serial.read();
        if (c == 'a') {
            processATcommand();
        }
        else if (c == 'l') {
            int ix[8];
            int n = smsList("REC UNREAD", ix, 8);
            Serial.printf("UNREAD SMS WAITING: %d\r\n", n);
        }
        else if (c == 'L') {
            int ix[8];
            int n = smsList("REC READ", ix, 8);
            Serial.printf("READ SMS WAITING: %d\r\n", n);
        }
        else if (c == 'r') {
            //smsRead();
            checkUnreadSMS();
        }
        else if (c == 'R') {
            //smsRead();
            checkReadSMS();
        }
        else if (c == 'd') {
            //smsDelete();
        }
        else if (c == 's') {
            //smsSend();
        }
        else if (c == 'h') {
            showHelp();
        }
        else {
            Serial.println("Bad command!");
        }
        while (Serial.available()) Serial.read(); // Flush the input buffer
    }
}

/*******************************************************************************
 * Function Name  : tinkerDigitalRead
 * Description    : Reads the digital value of a given pin
 * Input          : Pin
 * Output         : None.
 * Return         : Value of the pin (0 or 1) in INT type
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerDigitalRead(String pin)
{
    //convert ascii to integer
    int pinNumber = pin.charAt(1) - '0';
    //Sanity check to see if the pin numbers are within limits
    if (pinNumber < 0 || pinNumber > 7) return -1;

    if(pin.startsWith("D"))
    {
        pinMode(pinNumber, INPUT_PULLDOWN);
        return digitalRead(pinNumber);
    }
    else if (pin.startsWith("A"))
    {
        pinMode(pinNumber+10, INPUT_PULLDOWN);
        return digitalRead(pinNumber+10);
    }
#if Wiring_Cellular
    else if (pin.startsWith("B"))
    {
        if (pinNumber > 5) return -3;
        pinMode(pinNumber+24, INPUT_PULLDOWN);
        return digitalRead(pinNumber+24);
    }
    else if (pin.startsWith("C"))
    {
        if (pinNumber > 5) return -4;
        pinMode(pinNumber+30, INPUT_PULLDOWN);
        return digitalRead(pinNumber+30);
    }
#endif
    return -2;
}

/*******************************************************************************
 * Function Name  : tinkerDigitalWrite
 * Description    : Sets the specified pin HIGH or LOW
 * Input          : Pin and value
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerDigitalWrite(String command)
{
    bool value = 0;
    //convert ascii to integer
    int pinNumber = command.charAt(1) - '0';
    //Sanity check to see if the pin numbers are within limits
    if (pinNumber < 0 || pinNumber > 7) return -1;

    if(command.substring(3,7) == "HIGH") value = 1;
    else if(command.substring(3,6) == "LOW") value = 0;
    else return -2;

    if(command.startsWith("D"))
    {
        pinMode(pinNumber, OUTPUT);
        digitalWrite(pinNumber, value);
        return 1;
    }
    else if(command.startsWith("A"))
    {
        pinMode(pinNumber+10, OUTPUT);
        digitalWrite(pinNumber+10, value);
        return 1;
    }
#if Wiring_Cellular
    else if(command.startsWith("B"))
    {
        if (pinNumber > 5) return -4;
        pinMode(pinNumber+24, OUTPUT);
        digitalWrite(pinNumber+24, value);
        return 1;
    }
    else if(command.startsWith("C"))
    {
        if (pinNumber > 5) return -5;
        pinMode(pinNumber+30, OUTPUT);
        digitalWrite(pinNumber+30, value);
        return 1;
    }
#endif
    else return -3;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogRead
 * Description    : Reads the analog value of a pin
 * Input          : Pin
 * Output         : None.
 * Return         : Returns the analog value in INT type (0 to 4095)
                    Returns a negative number on failure
 *******************************************************************************/
int tinkerAnalogRead(String pin)
{
    //convert ascii to integer
    int pinNumber = pin.charAt(1) - '0';
    //Sanity check to see if the pin numbers are within limits
    if (pinNumber < 0 || pinNumber > 7) return -1;

    if(pin.startsWith("D"))
    {
        return -3;
    }
    else if (pin.startsWith("A"))
    {
        return analogRead(pinNumber+10);
    }
#if Wiring_Cellular
    else if (pin.startsWith("B"))
    {
        if (pinNumber < 2 || pinNumber > 5) return -3;
        return analogRead(pinNumber+24);
    }
#endif
    return -2;
}

/*******************************************************************************
 * Function Name  : tinkerAnalogWrite
 * Description    : Writes an analog value (PWM) to the specified pin
 * Input          : Pin and Value (0 to 255)
 * Output         : None.
 * Return         : 1 on success and a negative number on failure
 *******************************************************************************/
int tinkerAnalogWrite(String command)
{
    String value = command.substring(3);

    if(command.substring(0,2) == "TX")
    {
        pinMode(TX, OUTPUT);
        analogWrite(TX, value.toInt());
        return 1;
    }
    else if(command.substring(0,2) == "RX")
    {
        pinMode(RX, OUTPUT);
        analogWrite(RX, value.toInt());
        return 1;
    }

    //convert ascii to integer
    int pinNumber = command.charAt(1) - '0';
    //Sanity check to see if the pin numbers are within limits

    if (pinNumber < 0 || pinNumber > 7) return -1;

    if(command.startsWith("D"))
    {
        pinMode(pinNumber, OUTPUT);
        analogWrite(pinNumber, value.toInt());
        return 1;
    }
    else if(command.startsWith("A"))
    {
        pinMode(pinNumber+10, OUTPUT);
        analogWrite(pinNumber+10, value.toInt());
        return 1;
    }
    else if(command.substring(0,2) == "TX")
    {
        pinMode(TX, OUTPUT);
        analogWrite(TX, value.toInt());
        return 1;
    }
    else if(command.substring(0,2) == "RX")
    {
        pinMode(RX, OUTPUT);
        analogWrite(RX, value.toInt());
        return 1;
    }
#if Wiring_Cellular
    else if (command.startsWith("B"))
    {
        if (pinNumber > 3) return -3;
        pinMode(pinNumber+24, OUTPUT);
        analogWrite(pinNumber+24, value.toInt());
        return 1;
    }
    else if (command.startsWith("C"))
    {
        if (pinNumber < 4 || pinNumber > 5) return -4;
        pinMode(pinNumber+30, OUTPUT);
        analogWrite(pinNumber+30, value.toInt());
        return 1;
    }
#endif
    else return -2;
}

void processATcommand() {
    static bool once = false;
    if (!once) {
        once = true;
        Serial.println("Please be careful with AT commands. BACKSPACE and"
                   "\r\nESCAPE keys can be used to cancel and modify commands.");
    }
    Serial.print("Enter an AT command: ");
    int done = false;
    String cmd = "";
    while (!done) {
        if (Serial.available() > 0) {
            char c = Serial.read();
            if (c == '\r') {
                Serial.println();
                if(cmd != "") {
                    if (RESP_OK == Cellular.command("%s\r\n", cmd.c_str())) {
                        Serial.printlnf("%s :command request sent!", cmd.c_str());
                    }
                    else {
                        Serial.printlnf("%s :command request was not recognized by the modem!", cmd.c_str());
                    }
                }
                cmd = "";
                done = true;
            }
            else if (c == 27) { // ESC
                if (cmd.length() > 0) {
                    for (uint32_t x=0; x<cmd.length(); x++) {
                        Serial.print('\b');
                    }
                    for (uint32_t x=0; x<cmd.length(); x++) {
                        Serial.print(' ');
                    }
                    for (uint32_t x=0; x<cmd.length(); x++) {
                        Serial.print('\b');
                    }
                }
                Serial.println("command aborted!");
                cmd = "";
                done = true;
            }
            else if ((c == 8 || c == 127)) { // BACKSPACE
                if (cmd.length() > 0) {
                    cmd.remove(cmd.length()-1, 1);
                    Serial.print("\b \b");
                }
            }
            else {
                cmd += c;
                Serial.print(c);
            }
        }
        //Particle.process();
    }
}

void showHelp() {
    Serial.println("\r\nPress a key to run a command:"
                   "\r\n[l] list unread SMS"
                   "\r\n[L] list Read SMS"
                   "\r\n[d] delete SMS (not implemented)"
                   "\r\n[s] send SMS (not implemented)"
                   "\r\n[r] read unread SMS, do not delete or reply"
                   "\r\n[R] read already read SMS, delete and reply"
                   "\r\n[a] send an AT command"
                   "\r\n[h] show this help menu\r\n");
}
