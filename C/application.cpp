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

//SYSTEM_MODE(AUTOMATIC);
//SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_MODE(MANUAL);

uint32_t lastUpdate = 0;
bool D7state = false;

typedef struct { char* buf; char* num; } CMGRparam;
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

    
    if (type == TYPE_PLUS) {
        if (sscanf(buf, "\r\n+CMGR: \"%*[^\"]\",\"%[^\"]", param->num) == 1) {
            // extract number - if number is valid do ______?
        }
    } //else it must be a text message
    else if ((type == TYPE_UNKNOWN) && (len>2)) {
        memcpy(param->buf, buf, len);
        param->buf[len] = '\0';
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
    while (n-- > 0) //parse all messages in the queue
    {
        char num[32];
        Serial.printf("Unread SMS at index %d\r\n", ix[n]);
        if (smsRead(ix[n], num, buf, sizeof(buf))) {
            Serial.printf("Got SMS from \"%s\" with text \"%s\"\r\n", num, buf);
            // provide a generic default reply
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
    digitalWrite(D7, LOW);

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

/**********************************************************/
// CellLocate Functions
/**********************************************************/

