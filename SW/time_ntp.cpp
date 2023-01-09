/*-----------------------------------------------------------
NTP & time routines 

code based on Stefan Thesen 05/2015 - free for anyone
code for ntp adopted from Michael Margolis
code for time conversion based on http://stackoverflow.com/
-----------------------------------------------------------*/

// note: all timing relates to 01.01.2000

#include "time_ntp.h"

// NTP specifics
IPAddress timeServer(134,130,4,17); // time.nist.gov NTP server

unsigned int ntpPort = 2390;          // local port to listen for UDP packets
const int NTP_PACKET_SIZE = 48;       // NTP time stamp is in the first 48 bytes of the message

// ntp flag
bool bNTPStarted=false;

char timebuffer[40];

static unsigned short days[4][12] =
{
    {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
    { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
    { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
    {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};


unsigned int date_time_to_epoch(date_time_t* date_time)
{
    unsigned int second = date_time->second;  // 0-59
    unsigned int minute = date_time->minute;  // 0-59
    unsigned int hour   = date_time->hour;    // 0-23
    unsigned int day    = date_time->day-1;   // 0-30
    unsigned int month  = date_time->month-1; // 0-11
    unsigned int year   = date_time->year;    // 0-99
    return (((year/4*(365*4+1)+days[year%4][month]+day)*24+hour)*60+minute)*60+second;
}

unsigned long getNTPTimestamp()
{
  unsigned long ulSecs2000;
  WiFiUDP udp;  // A UDP instance to let us send and receive packets over UDP
  byte packetBuffer[ NTP_PACKET_SIZE];  //buffer to hold incoming and outgoing packets
  
  udp.begin(ntpPort);
  //sendNTPpacket(timeServer); // send an NTP packet to a time server

  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  udp.beginPacket(timeServer, 123); //NTP requests are to port 123
  udp.write(packetBuffer, NTP_PACKET_SIZE);
  udp.endPacket();

  delay(1000);    // wait to see if a reply is available
  int cb=0, repeat=0; 
  while(!cb && repeat<20)  // try for 2 sec
  {
    cb = udp.parsePacket();
    delay(100);
    repeat++;
  }

  if(!cb)
  {
    ulSecs2000=0;
  }
  else
  {
    udp.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
    
    //the timestamp starts at byte 40 of the received packet and is four bytes,
    // or two words, long. First, esxtract the two words:
    unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
    unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
    
    // combine the four bytes (two words) into a long integer
    // this is NTP time (seconds since Jan 1 1900):
    ulSecs2000  = highWord << 16 | lowWord;
    ulSecs2000 -= 2208988800UL; // go from 1900 to 1970
    ulSecs2000 -= 946684800UL; // go from 1970 to 2000
  }    
  return(ulSecs2000);
}



void epoch_to_date_time(date_time_t* date_time,unsigned int epoch)
{
    date_time->second = epoch%60; epoch /= 60;
    date_time->minute = epoch%60; epoch /= 60;
    date_time->hour   = epoch%24; epoch /= 24;

    unsigned int years = epoch/(365*4+1)*4; epoch %= 365*4+1;

    unsigned int year;
    for (year=3; year>0; year--)
    {
        if (epoch >= days[year][0])
            break;
    }

    unsigned int month;
    for (month=11; month>0; month--)
    {
        if (epoch >= days[year][month])
            break;
    }

    date_time->year  = years+year;
    date_time->month = month+1;
    date_time->day   = epoch-days[year][month]+1;
}

String epoch_to_string(unsigned int epoch)
{
  date_time_t date_time;
  epoch_to_date_time(&date_time,epoch);
  String s;
  int i;
  
  s = date_time.hour; 
  s+= ":";
  i=date_time.minute;
  if (i<10) {s+= "0";}
  s+= i;
  s+= ":";
  i=date_time.second;
  if (i<10) {s+= "0";}
  s+= i;
  s+= " - ";
  s+= date_time.day;
  s+= ".";
  s+= date_time.month;
  s+= ".";
  s+= 2000+(int)date_time.year;
  
  return(s);
}

//////////////////////////////////////////
// for time conversion
//Central European Time (Frankfurt, Paris)
//////////////////////////////////////////
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone CE(CEST, CET);

///////////////////////////////////////////
// get UTC time referenced to 1970 by NTP
///////////////////////////////////////////
time_t getNTP_UTCTime1970() 
{ 
  bNTPStarted=false;  // invalidate; time-lib functions crash, if not initalized poperly
  unsigned long t = getNTPTimestamp();
  if (t==0) return(0);

  // scale to 1970 
  // may look like back & forth with ntp code; wrote it to make needed conversions more clear
  return(t+946684800UL);
}

///////////////////////////////////////////
// Get current NTP time and encode as string
///////////////////////////////////////////
void ntptimetostring()
{
  time_t tT = now();
  delay(20);
  time_t tTlocal = CE.toLocal(tT);
  delay(20);
  sprintf(timebuffer, "%d/%d/%d %d:%d:%d", day(tTlocal), month(tTlocal), year(tTlocal), hour(tTlocal), minute(tTlocal), second(tTlocal));
  Serial.println(timebuffer);
}
