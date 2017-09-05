

import socket
import time

#NICK='anickname'
#oath_token='oauth:xxxxxxxxxxxxxxxxxxxxxx'
#channel='yourchannel'
#host='irc.chat.twitch.tv'
#port=6667
from cfg import *

oath_str='PASS {0}\r\n'.format(oath_token).encode("utf-8")
nick_str='NICK {0}\r\n'.format(NICK).encode("utf-8")
join_str='JOIN #{0}\r\n'.format(channel).encode("utf-8")

class Twillbot():
    def __init__(self):
        self.s = socket.socket()
        self._socketConnect()
        
    def _socketConnect(self):
        print "("+host+","+str(port)+")"
        self.s.connect((host,port))
        self.s.send(oath_str)
        self.s.send(nick_str)
        self.s.send(join_str)

    def _sendMessage(self, message):
       self.s.send("PRIVMSG #{0} :{1}\r\n".format(channel,message).encode("utf-8"))

    def readSocket(self):
        #self._sendMessage("!rank2")
        #response = self.s.recv(2048).decode("utf-8")
        #print response
        lockout=0
        gusher_count=0
        loop_counter=0
        start_time=time.time()

        while True:
            loop_counter+=1
            response = self.s.recv(2048).decode("utf-8")
            if response == "PING :tmi.twitch.tv\r\n": #keepalive
                self.s.send("PONG :tmi.twitch.tv\r\n".encode("utf-8"))
            #print response

            gusher_count+=response.count("!gusher")
            loop_time=time.time()-start_time
            if (loop_time > 3):
                if lockout:
                    lockout=lockout-1 
                else:
                    if gusher_count>=10:
                        self.s.send("!gusher")
                        lockout=5
                        
                print "messages counted #{0} or >{1} seconds running".format(loop_counter, loop_counter*3)
                print "GUSHERS {0} (seen in the last ~3 seconds)".format(gusher_count)
                gusher_count=0
                start_time=time.time()

        self.s.close()


if __name__ == "__main__":
    print "start"
    Twill=Twillbot()
    print "did it work?"
    Twill.readSocket()
    
