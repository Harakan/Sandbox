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
#include "cellular_hal.h"

PRODUCT_ID(PLATFORM_ID);
PRODUCT_VERSION(2);

// ALL_LEVEL, TRACE_LEVEL, DEBUG_LEVEL, INFO_LEVEL, WARN_LEVEL, ERROR_LEVEL, PANIC_LEVEL, NO_LOG_LEVEL
SerialDebugOutput debugOutput(115200, ALL_LEVEL);

STARTUP(cellular_credentials_set("pda.bell.ca", "", "", NULL)); // BELL
//STARTUP(cellular_credentials_set("broadband", "", "", NULL)); // AT&T

//SYSTEM_MODE(AUTOMATIC);
//SYSTEM_MODE(SEMI_AUTOMATIC);
SYSTEM_MODE(MANUAL);


/**********************************************************/
// CellLocate Functions
/**********************************************************/
struct MDM_CELL_LOCATE {
    int day;
    int month;
    int year;
    int hour;
    int minute;
    int second;
    char lat[14];
    char lng[14];
    int altitude; // only for GNSS positioning
    int uncertainty;
    int speed; // only for GNSS positioning
    int direction; // only for GNSS positioning
    int vertical_acc; // only for GNSS positioning
    int sensor_used; // 0: the last fix in the internal database, 2: CellLocate(R) location information
    int sv_used; // only for GNSS positioning
    int antenna_status; // only for GNSS positioning
    int jamming_status; // only for GNSS positioning
    int count;
    bool ok;
    int size;

    MDM_CELL_LOCATE()
    {
        memset(this, 0, sizeof(*this));
        size = sizeof(*this);
    }
};

MDM_CELL_LOCATE _cell_locate;
bool displayed_once = false;
volatile uint32_t cellTimeout;
volatile uint32_t cellTimeStart;

void cell_locate_timeout_set(uint32_t timeout_ms) {
    cellTimeout = timeout_ms;
    cellTimeStart = millis();
}

bool is_cell_locate_timeout() {
    return (cellTimeout && ((millis()-cellTimeStart) > cellTimeout));
}

void cell_locate_timeout_clear() {
    cellTimeout = 0;
}

bool is_cell_locate_matched(MDM_CELL_LOCATE& loc) {
    return loc.ok;
}

/* Cell Locate Callback */
int _cbLOCATE(int type, const char* buf, int len, MDM_CELL_LOCATE* data)
{
    if ((type == TYPE_PLUS) && data) {
        // DEBUG CODE TO SEE EACH LINE PARSED
        // char line[256];
        // strncpy(line, buf, len);
        // line[len] = '\0';
        // Serial.printf("LINE: %s",line);

        // <response_type> = 1:
        //+UULOC: <date>,<time>,<lat>,<long>,<alt>,<uncertainty>,<speed>,<direction>,
        //        <vertical_acc>,<sensor_used>,<SV_used>,<antenna_status>,<jamming_status>
        //+UULOC: 25/09/2013,10:13:29.000,45.7140971,13.7409172,266,17,0,0,18,1,6,3,9
        int count = 0;
        //
        // TODO: %f was not working for float on LAT/LONG, so opted for capturing strings for now
        if ( (count = sscanf(buf, "\r\n+UULOC: %d/%d/%d,%d:%d:%d.%*d,%[^,],%[^,],%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n",
            &data->day,
            &data->month,
            &data->year,
            &data->hour,
            &data->minute,
            &data->second,
            data->lat,
            data->lng,
            &data->altitude,
            &data->uncertainty,
            &data->speed,
            &data->direction,
            &data->vertical_acc,
            &data->sensor_used,
            &data->sv_used,
            &data->antenna_status,
            &data->jamming_status) ) > 0 ) {
            // UULOC Matched
            data->count = count;
            data->ok = true;
        }
    }
    return WAIT;
}

int cell_locate(MDM_CELL_LOCATE& loc, uint32_t timeout_ms) {
    loc.count = 0;
    loc.ok = false;
    if (RESP_OK == Cellular.command(5000, "AT+ULOCCELL=0\r\n")) {
        if (RESP_OK == Cellular.command(_cbLOCATE, &loc, timeout_ms, "AT+ULOC=2,2,1,%d,5000\r\n", timeout_ms/1000)) {
            cell_locate_timeout_set(timeout_ms);
            if (loc.count > 0) {
                return loc.count;
            }
            return 0;
        }
        else {
            return -2;
            // Serial.println("Error! No Response from AT+LOC");
        }
    }
    // else Serial.println("Error! No Response from AT+ULOCCELL");
    return -1;
}

bool cell_locate_in_progress(MDM_CELL_LOCATE& loc) {
    if (!is_cell_locate_matched(loc) && !is_cell_locate_timeout()) {
        return true;
    }
    else {
        cell_locate_timeout_clear();
        return false;
    }
}

bool cell_locate_get_response(MDM_CELL_LOCATE& loc) {
    // Send empty string to check for URCs that were slow
    Cellular.command(_cbLOCATE, &loc, 1000, "");
    if (loc.count > 0) {
        return true;
    }
    return false;
}

void cell_locate_display(MDM_CELL_LOCATE& loc) {
    /* The whole kit-n-kaboodle */
    Serial.printlnf("\r\n%d/%d/%d,%d:%d:%d,LAT:%s,LONG:%s,%d,UNCERTAINTY:%d,SPEED:%d,%d,%d,%d,%d,%d,%d,MATCHED_COUNT:%d",
        loc.month,
        loc.day,
        loc.year,
        loc.hour,
        loc.minute,
        loc.second,
        loc.lat,
        loc.lng,
        loc.altitude,
        loc.uncertainty,
        loc.speed,
        loc.direction,
        loc.vertical_acc,
        loc.sensor_used,
        loc.sv_used,
        loc.antenna_status,
        loc.jamming_status,
        loc.count);

    /* A nice map URL */
    Serial.printlnf("\r\nhttps://www.google.com/maps?q=%s,%s\r\n",loc.lat,loc.lng);
    Serial.printlnf("https://www.google.com/maps?q=%s,%s Accuracy ~= %d meters\r\n", loc.lat, loc.lng, loc.uncertainty); ;
}


/**********************************************************/
// SMS functions
/**********************************************************/

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
    char tmpReply[64] = "";
    int ix[8];
    int n = smsList("REC UNREAD", ix, 8);
    int retcode;
    bool sendStatus=0;
    bool foundLoc=0;
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
            if (strstr(buf, /*w*/"ho are you")){
                reply = "I am the automated AI in the SusText - written by Will!";
            }
            
            if (strstr(buf, /*w*/"here are you")){
                Serial.printf("GOT where are you\r\n");    
                foundLoc=0;
                reply="Couldn't get triangulation fix, try again?";
                retcode = cell_locate(_cell_locate, 10*1000); 
                if (retcode >= 8){
                    Serial.printf("INSTANT RESPONSE\r\n");    
                    sprintf(tmpReply, "https://www.google.com/maps?q=%s,%s Accuracy ~= %d meters\r\n", _cell_locate.lat, _cell_locate.lng, _cell_locate.uncertainty); ;
                    reply = tmpReply;
                    Serial.printf("Reply: %s\r\n",reply);    
                    foundLoc=1;
                }
                else if (retcode == 0) {
                    /* retcode == 0, still waiting for the URC */
                    while (cell_locate_in_progress(_cell_locate)) {
                        /* still waiting for URC */
                        if (cell_locate_get_response(_cell_locate)) {
                            Serial.printf("LATE RESPONSE\r\n");    
                            sprintf(tmpReply, "https://www.google.com/maps?q=%s,%s Accuracy ~= %d meters\r\n", _cell_locate.lat, _cell_locate.lng, _cell_locate.uncertainty); ;
                            reply = tmpReply;
                            Serial.printf("Reply: %s\r\n",reply);    
                            foundLoc=1;
                        }
                    }
                }
                Serial.printf("Got Reply: %s\r\n",reply);    
                Serial.printf("Got Return %d\r\n",foundLoc);    
            }
                

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

/**********************************************************/
// Main functions
/**********************************************************/

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
    //resp = Cellular.command(1000, "AT+CMGF=1\r\n");
    //Serial.printf("Cellular connected, set mode to text: %d (-2==OK)\r\n",resp);
    digitalWrite(D7, LOW);

}

/* This function loops forever --------------------------------------------*/
void loop()
{
    static int retcode=0;
    
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
        else if (c == 'p') {
            retcode = cell_locate(_cell_locate, 10*1000); 
            Serial.printf("CELL TRIANGULATION returned: %d\r\n", retcode);
            if (retcode >= 8){
                cell_locate_display(_cell_locate);
            }
            else if (retcode == 0) {
                /* ret == 0, still waiting for the URC
                 * Check for cell locate response, and display it. */
                Serial.print("Waiting for URC ");
                while (cell_locate_in_progress(_cell_locate)) {
                    /* still waiting for URC */
                    if (cell_locate_get_response(_cell_locate)) {
                        cell_locate_display(_cell_locate);
                    }
                }
            }
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
                   "\r\n[p] calculate and display celllocate"
                   "\r\n[a] send an AT command"
                   "\r\n[h] show this help menu\r\n");
}


