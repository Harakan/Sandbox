#!/bin/bash

#Page 1052 of Core bluetooth V4.0 spec:

#Basic hcitool usage
#hcitool -i hci0 cmd $OGF $OCF_SET_ADVERTISING_PARAMS

#OGF=0x08 # OpCode Group Field: 
    #page 1052 - "For the LE Controller Commands, the OGF code is defined as 0x08"

#OCF=0x0008 #OpCode Command Field 
    #Set Event Mask = 0x0001
    #Read Buffer Size = 0x0002
    #Read Local Supported Features Command = 0x0003
    #Set Random Address Command = 0x0005
    #Set Advertising Parameters Command = 0x0006
        #Interval_min
        #Interval_max
        #Advertising_type
        #Own_address_type
        #Direct_address_type
        #Direct_address
        #Advertising_channel_map
        #Advertising_filter_policy
    #Read Advertising Channel Tx Power = 0x0007
    #Set Advertising Data Command = 0x0008
        #Advertising_Data_Length
        #Advertising_data
        #NOTE: This should be zero padded to 32 bytes
    #Set Scan Response Data Command = 0x0009
    #Set Advertise Enable Command = 0x000A
    #Set Scan Parameters Command = 0x000B
    #Set Scan Enable Command = 0x000C
    #Create Connection Command = 0x000D
    #Create Connection Cancel = 0x000E
    #Transmitter Test Command = 0x001E
        #TX_Frequency
        #Length of test data
        #Packet payload
    #Test End Command = 0x001F

#OGF: 
OGF_ANNOUNCE="0x08"
#https://www.bluetooth.org/en-us/specification/assigned-numbers/generic-access-profile

TYPE_SHORT_NAME="0x08"
TYPE_TXPOWER="0x0A" #(Opcode Group Field)
TYPE_FLAGS="0x01"
TYPE_DEVICE_ID="0x10"


OCF_SET_ADVERTISING_DATA="0x0008" #Set Advertising Data (Opcode Command Field)
OCF_SET_ADVERTISING_PARAMS="0x0006" #Set Advertising Data (Opcode Command Field)

#Advertising params:
AD_INTERVAL_MIN="0x0400" #(N*0.625ms)
AD_INTERVAL_MAX="0x0400" #(N*0.625ms)
AD_TYPE_PARAM="0x00" #(0x03-non_connectable, 0x00-connectable)
AD_OWN_ADDRESS="0x00" #(0x00-Public_address 0x01-Random_Devicer_address)
AD_DIRECT_ADDRESS_TYPE="0x00" #(0x00-Public_address 0x01-Random_Devicer_address)
AD_DIRECT_ADDRESS="0xF0F0F0F0F0F0"
AD_CHAN_MAP="0x07" #7=Default, all channels
AD_FILTER="0x0" #Allow scans from anyone

AD_128UUID_CMD="21"
AD_128UUID="00 11 22 33 44 55 66 77 88 99 AA BB CC DD EE FF"
AD_128UUID_LENGTH="11"
AD_128UUID_TOTAL_LENGTH=17

AD_NAME_CMD="09"
AD_NAME="6f 6e 61 42 5f 52 41 65 73 75 4d"
AD_NAME_LENGTH="0c"
AD_NAME_TOTAL_LENGTH=13

#Advertising Message
#Page 1735 of V4.0 manual
#Page 1761 of V4.0 manual

echo "Starting to set advertising parameters"
hciconfig hci0 down
hciconfig hci0 up

#hcitool -i hci0 cmd $OGF_ANNOUNCE $OCF_SET_ADVERTISING_PARAMS\
# $AD_INTERVAL_MIN $AD_INTERVAL_MAX $AD_TYPE_PARAM $AD_OWN_ADDRESS\
# $AD_DIRECT_ADDRESS_TYPE $AD_DIRECT_ADDRESS\
# $AD_CHAN_MAP $AD_FILTER

echo "Got Return code for setting params: "$?

#page 1738 of V4.0 spec for example:

ANNOUNCE_LENGTH="12" #0x11 for 128b uuid, 0x
hcitool -i hci0 cmd $OGF_ANNOUNCE $OCF_SET_ADVERTISING_DATA \
 $ANNOUNCE_LENGTH \
 $AD_128UUID_LENGTH $AD_128UUID_CMD $AD_128UUID \
 #$AD_NAME_LENGTH $AD_NAME_CMD $AD_NAME

#[TOTAL LENGTH][<individual_length><ad_type><ad_payload>][...etc...]

hciconfig hci0 leadv 3

echo "Should be advertising now!"

#32 bytes:
#4 bytes - UNIQUE MUSEAR
#4 bytes - ROOM IDENTIFIER
#1 byte - ROOM BEACON NUMBER
