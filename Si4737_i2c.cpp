/* 
UWAFT Si4737 Stuff

Functions will be stolen from the Si4735 library as needed
*/

//Appetizer
#include "Si4737_i2c.h"
#include "Si4737_i2c-private.h"
#include <Wire.h>

//Main Course
void Si4737RDSDecoder::decodeRDSBlock(word block[]){
	byte grouptype;
	word fourchars[2];

	_status.programIdentifier = block[0];
	grouptype = lowByte((block[1] & SI4735_RDS_TYPE_MASK) >>
		SI4735_RDS_TYPE_SHR);
	_status.TP = block[1] & SI4735_RDS_TP;
	_status.PTY = lowByte((block[1] & SI4735_RDS_PTY_MASK) >>
		SI4735_RDS_PTY_SHR);
#if defined(SI4735_DEBUG)
	_rdsstats[grouptype]++;
#endif

	switch(grouptype){
	case SI4735_GROUP_0A:
	case SI4735_GROUP_0B:
	case SI4735_GROUP_15B:
		byte DIPSA;
		word twochars;

		_status.TA = block[1] & SI4735_RDS_TA;
		_status.MS = block[1] & SI4735_RDS_MS;
		DIPSA = lowByte(block[1] & SI4735_RDS_DIPS_ADDRESS);
		bitWrite(_status.DICC, 3 - DIPSA, block[1] & SI4735_RDS_DI);
		twochars = switchEndian(block[3]);
		strncpy(&_status.programService[DIPSA * 2], (char *)&twochars, 2);
		if(grouptype == SI4735_GROUP_0A) {
			//TODO: read the standard and do AF list decoding
		}
		break;
	case SI4735_GROUP_1A:
	case SI4735_GROUP_1B:
		//TODO: read the standard and do PIN and slow labeling codes
		break;
	case SI4735_GROUP_2A:
	case SI4735_GROUP_2B:
		byte RTA, RTAW;

		if((block[1] & SI4735_RDS_TEXTAB) != _rdstextab) {
			_rdstextab = !_rdstextab;
			memset(_status.radioText, ' ', 64);
		}
		RTA = lowByte(block[1] & SI4735_RDS_TEXT_ADDRESS);
		RTAW = (grouptype == SI4735_GROUP_2A) ? 4 : 2;
		fourchars[0] = switchEndian(
			block[(grouptype == SI4735_GROUP_2A) ? 2 : 3]);
		if(grouptype == SI4735_GROUP_2A)
			fourchars[1] = switchEndian(block[3]);
		strncpy(&_status.radioText[RTA * RTAW], (char *)fourchars, RTAW);
		break;
	case SI4735_GROUP_3A:
		//TODO: read the standard and do AID listing
		break;
	case SI4735_GROUP_3B:
	case SI4735_GROUP_4B:
	case SI4735_GROUP_6A:
	case SI4735_GROUP_6B:
	case SI4735_GROUP_7B:
	case SI4735_GROUP_8B:
	case SI4735_GROUP_9B:
	case SI4735_GROUP_10B:
	case SI4735_GROUP_11A:
	case SI4735_GROUP_11B:
	case SI4735_GROUP_12A:
	case SI4735_GROUP_12B:
	case SI4735_GROUP_13B:
		//Application data payload (ODA), ignore for now
		break;
	case SI4735_GROUP_4A:
		unsigned long MJD, CT, ys;
		word yp;
		byte k, mp;

		CT = ((unsigned long)block[2] << 16) | block[3];
		//The standard mandates that CT must be all zeros if no time
		//information is being provided by the current station.
		if(!CT) break;

		_havect = true;
		MJD = (unsigned long)(block[1] & SI4735_RDS_MJD_MASK) <<
			SI4735_RDS_MJD_SHL;
		MJD |= (CT & SI4735_RDS_TIME_MJD_MASK) >> SI4735_RDS_TIME_MJD_SHR;

		//We report UTC for now, better not fiddle with timezones
		_time.tm_hour = (CT & SI4735_RDS_TIME_HOUR_MASK) >>
			SI4735_RDS_TIME_HOUR_SHR;
		_time.tm_min = (CT & SI4735_RDS_TIME_MINUTE_MASK) >>
			SI4735_RDS_TIME_MINUTE_SHR;
		//Use integer arithmetic at all costs, Arduino lacks an FPU
		yp = (MJD * 10 - 150782) * 10 / 36525;
		ys = yp * 36525 / 100;
		mp = (MJD * 10 - 149561 - ys * 10) * 1000 / 306001;
		_time.tm_mday = MJD - 14956 - ys - mp * 306001 / 10000;
		k = (mp == 14 || mp == 15) ? 1 : 0;
		_time.tm_year = 1900 + yp + k;
		_time.tm_mon = mp - 1 - k * 12;
		_time.tm_wday = (MJD + 2) % 7 + 1;
		break;
	case SI4735_GROUP_5A:
	case SI4735_GROUP_5B:
		//TODO: read the standard and do TDC listing
		break;
	case SI4735_GROUP_7A:
		//TODO: read the standard and do Radio Paging
		break;
	case SI4735_GROUP_8A:
		//TODO: read the standard and do TMC listing
		break;
	case SI4735_GROUP_9A:
		//TODO: read the standard and do EWS listing
		break;
	case SI4735_GROUP_10A:
		if((block[1] & SI4735_RDS_PTYNAB) != _rdsptynab) {
			_rdsptynab = !_rdsptynab;
			memset(_status.programTypeName, ' ', 50);
		}
		fourchars[0] = switchEndian(block[2]);
		fourchars[1] = switchEndian(block[3]);
		strncpy(&_status.programTypeName[(block[1] &
			SI4735_RDS_PTYN_ADDRESS) * 4],
			(char *)&fourchars, 4);
		break;
	case SI4735_GROUP_13A:
		//TODO: read the standard and do Enhanced Radio Paging
		break;
	case SI4735_GROUP_14A:
	case SI4735_GROUP_14B:
		//TODO: read the standard and do EON listing
		break;
	case SI4735_GROUP_15A:
		//Withdrawn and currently unallocated, ignore
		break;
	}
}

void Si4737RDSDecoder::getRDSData(Si4737_RDS_Data* rdsdata){
	makePrintable(_status.programService);
	makePrintable(_status.programTypeName);
	makePrintable(_status.radioText);

	*rdsdata = _status;
}

bool Si4737RDSDecoder::getRDSTime(Si4737_RDS_Time* rdstime){
	if(_havect && rdstime) *rdstime = _time;

	return _havect;
}

void Si4737RDSDecoder::resetRDS(void){
	memset(_status.programService, ' ', 8);
	_status.programService[8] = '\0';
	memset(_status.programTypeName, ' ', 50);
	_status.programTypeName[50] = '\0';
	memset(_status.radioText, ' ', 64);
	_status.radioText[64] = '\0';
	_status.DICC = 0;
	_rdstextab = false;
	_rdsptynab = false;
	_havect = false;
#if defined(SI4735_DEBUG)
	memset((void *)&_rdsstats, 0x00, sizeof(_rdsstats));
#endif
}

void Si4737RDSDecoder::makePrintable(char* str){
	for(byte i = 0; i < strlen(str); i++) {
		if(str[i] == 0x0D) {
			str[i] = '\0';
			break;
		}
		if(str[i] < 32 || str[i] > 126) str[i] = '?';
	}
}

#if defined(SI4735_DEBUG)
void Si4735RDSDecoder::dumpRDSStats(void){
	SerialUSB.println("RDS group statistics:");
	for(byte i = 0; i < 32; i++) {
		SerialUSB.print("#");
		SerialUSB.print(i >> 1);
		SerialUSB.print((i & 0x01) ? 'B' : 'A');
		SerialUSB.print(": ");
		SerialUSB.println(_rdsstats[i]);
	}
	SerialUSB.flush();
}
#endif

const char Si4735_PTY2Text_S_None[] PROGMEM = "None/Undefined";
const char Si4735_PTY2Text_S_News[] PROGMEM = "News";
const char Si4735_PTY2Text_S_Current[] PROGMEM = "Current affairs";
const char Si4735_PTY2Text_S_Information[] PROGMEM = "Information";
const char Si4735_PTY2Text_S_Sports[] PROGMEM = "Sports";
const char Si4735_PTY2Text_S_Education[] PROGMEM = "Education";
const char Si4735_PTY2Text_S_Drama[] PROGMEM = "Drama";
const char Si4735_PTY2Text_S_Culture[] PROGMEM = "Culture";
const char Si4735_PTY2Text_S_Science[] PROGMEM = "Science";
const char Si4735_PTY2Text_S_Varied[] PROGMEM = "Varied";
const char Si4735_PTY2Text_S_Pop[] PROGMEM = "Pop";
const char Si4735_PTY2Text_S_Rock[] PROGMEM = "Rock";
const char Si4735_PTY2Text_S_EasySoft[] PROGMEM = "Easy & soft";
const char Si4735_PTY2Text_S_Classical[] PROGMEM = "Classical";
const char Si4735_PTY2Text_S_Other[] PROGMEM = "Other music";
const char Si4735_PTY2Text_S_Weather[] PROGMEM = "Weather";
const char Si4735_PTY2Text_S_Finance[] PROGMEM = "Finance";
const char Si4735_PTY2Text_S_Children[] PROGMEM = "Children's";
const char Si4735_PTY2Text_S_Social[] PROGMEM = "Social affairs";
const char Si4735_PTY2Text_S_Religion[] PROGMEM = "Religion";
const char Si4735_PTY2Text_S_TalkPhone[] PROGMEM = "Talk & phone-in";
const char Si4735_PTY2Text_S_Travel[] PROGMEM = "Travel";
const char Si4735_PTY2Text_S_Leisure[] PROGMEM = "Leisure";
const char Si4735_PTY2Text_S_Jazz[] PROGMEM = "Jazz";
const char Si4735_PTY2Text_S_Country[] PROGMEM = "Country";
const char Si4735_PTY2Text_S_National[] PROGMEM = "National";
const char Si4735_PTY2Text_S_Oldies[] PROGMEM = "Oldies";
const char Si4735_PTY2Text_S_Folk[] PROGMEM = "Folk";
const char Si4735_PTY2Text_S_Documentary[] PROGMEM = "Documentary";
const char Si4735_PTY2Text_S_EmergencyTest[] PROGMEM = "Emergency test";
const char Si4735_PTY2Text_S_Emergency[] PROGMEM = "Emergency";
const char Si4735_PTY2Text_S_Adult[] PROGMEM = "Adult hits";
const char Si4735_PTY2Text_S_Top40[] PROGMEM = "Top 40";
const char Si4735_PTY2Text_S_Nostalgia[] PROGMEM = "Nostalgia";
const char Si4735_PTY2Text_S_RnB[] PROGMEM = "Rhythm and blues";
const char Si4735_PTY2Text_S_Language[] PROGMEM = "Language";
const char Si4735_PTY2Text_S_Personality[] PROGMEM = "Personality";
const char Si4735_PTY2Text_S_Public[] PROGMEM = "Public";
const char Si4735_PTY2Text_S_College[] PROGMEM = "College";

const char * const Si4735_PTY2Text_EU[32] PROGMEM = {
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_News,
	Si4735_PTY2Text_S_Current,
	Si4735_PTY2Text_S_Information,
	Si4735_PTY2Text_S_Sports,
	Si4735_PTY2Text_S_Education,
	Si4735_PTY2Text_S_Drama,
	Si4735_PTY2Text_S_Culture,
	Si4735_PTY2Text_S_Science,
	Si4735_PTY2Text_S_Varied,
	Si4735_PTY2Text_S_Pop,
	Si4735_PTY2Text_S_Rock,
	Si4735_PTY2Text_S_EasySoft,
	Si4735_PTY2Text_S_Classical,
	Si4735_PTY2Text_S_Classical,
	Si4735_PTY2Text_S_Other,
	Si4735_PTY2Text_S_Weather,
	Si4735_PTY2Text_S_Finance,
	Si4735_PTY2Text_S_Children,
	Si4735_PTY2Text_S_Social,
	Si4735_PTY2Text_S_Religion,
	Si4735_PTY2Text_S_TalkPhone,
	Si4735_PTY2Text_S_Travel,
	Si4735_PTY2Text_S_Leisure,
	Si4735_PTY2Text_S_Jazz,
	Si4735_PTY2Text_S_Country,
	Si4735_PTY2Text_S_National,
	Si4735_PTY2Text_S_Oldies,
	Si4735_PTY2Text_S_Folk,
	Si4735_PTY2Text_S_Documentary,
	Si4735_PTY2Text_S_EmergencyTest,
	Si4735_PTY2Text_S_Emergency};

const char * const Si4735_PTY2Text_US[32] PROGMEM = {
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_News,
	Si4735_PTY2Text_S_Information,
	Si4735_PTY2Text_S_Sports,
	Si4735_PTY2Text_S_TalkPhone,
	Si4735_PTY2Text_S_Rock,
	Si4735_PTY2Text_S_Rock,
	Si4735_PTY2Text_S_Adult,
	Si4735_PTY2Text_S_Rock,
	Si4735_PTY2Text_S_Top40,
	Si4735_PTY2Text_S_Country,
	Si4735_PTY2Text_S_Oldies,
	Si4735_PTY2Text_S_EasySoft,
	Si4735_PTY2Text_S_Nostalgia,
	Si4735_PTY2Text_S_Jazz,
	Si4735_PTY2Text_S_Classical,
	Si4735_PTY2Text_S_RnB,
	Si4735_PTY2Text_S_RnB,
	Si4735_PTY2Text_S_Language,
	Si4735_PTY2Text_S_Religion,
	Si4735_PTY2Text_S_Religion,
	Si4735_PTY2Text_S_Personality,
	Si4735_PTY2Text_S_Public,
	Si4735_PTY2Text_S_College,
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_None,
	Si4735_PTY2Text_S_Weather,
	Si4735_PTY2Text_S_EmergencyTest,
	Si4735_PTY2Text_S_Emergency};

const byte Si4735_PTY_EU2US[32] PROGMEM = {0, 1, 0, 2, 3, 23, 0, 0, 0, 0, 7,
	5, 12, 15, 15, 0, 29, 0, 0, 0, 20,
	4, 0, 0, 14, 10, 0, 11, 0, 0, 30,
	31};
const byte Si4735_PTY_US2EU[32] PROGMEM = {0, 1, 3, 4, 21, 11, 11, 10, 11, 10,
	25, 27, 12, 27, 24, 14, 15, 15, 0,
	20, 20, 0, 0, 5, 0, 0, 0, 0, 0, 16,
	30, 31};

void Si4737Translate::getTextForPTY(byte PTY, byte locale, char* text,
									byte textsize){
										switch(locale){
										case SI4735_LOCALE_US:
											strncpy_P(text, (PGM_P)(pgm_read_word(&Si4735_PTY2Text_US[PTY])),
												textsize);
											break;
										case SI4735_LOCALE_EU:
											strncpy_P(text, (PGM_P)(pgm_read_word(&Si4735_PTY2Text_EU[PTY])),
												textsize);
											break;
										}
}

byte Si4737Translate::translatePTY(byte PTY, byte fromlocale, byte tolocale){
	if(fromlocale == tolocale) return PTY;
	else switch(fromlocale){
	case SI4735_LOCALE_US:
		return pgm_read_byte(&Si4735_PTY_US2EU[PTY]);
		break;
	case SI4735_LOCALE_EU:
		return pgm_read_byte(&Si4735_PTY_EU2US[PTY]);
		break;
	}

	//Never reached
	return 0;
}

void Si4737Translate::decodeCallSign(word programIdentifier, char* callSign){
	//TODO: read the standard and implement world-wide PI decoding
	if(programIdentifier >= 21672){
		callSign[0] = 'W';
		programIdentifier -= 21672;
	} else
		if(programIdentifier < 21672 && programIdentifier >= 0x1000){
			callSign[0] = 'K';
			programIdentifier -= 0x1000;
		} else programIdentifier -= 1;
	if(programIdentifier >= 0){
		callSign[1] = char(programIdentifier / 676 + 'A');
		callSign[2] = char((programIdentifier - 676 * programIdentifier /
			676) / 26 + 'A');
		callSign[3] = char(((programIdentifier - 676 * programIdentifier /
			676) % 26 ) + 'A');
		callSign[4] = '\0';
	} else strcpy(callSign, "UNKN");
}


Si4737::Si4737(byte partNumberLastTwo, byte interface, byte pinPower, byte pinReset, byte pinGPO2,
			   byte pinSEN){
                                   _partNumberLastTwo = partNumberLastTwo;
				   _mode = SI4735_MODE_FM;
				   _pinPower = pinPower;
				   _pinReset = pinReset;
				   _pinGPO2 = pinGPO2;
				   _pinSEN = pinSEN;
				   switch(interface){
				   case SI4735_INTERFACE_SPI:
					   _i2caddr = 0x00;
					   break;
				   case SI4735_INTERFACE_I2C:
					   if(_pinSEN == SI4735_PIN_SEN_HWH) _i2caddr = SI4735_I2C_ADDR_H;
					   else _i2caddr = SI4735_I2C_ADDR_L;
					   break;
				   }
}

void Si4737::end(bool hardoff){
	sendCommand(SI4735_CMD_POWER_DOWN);
	if(hardoff) {
		//datasheet calls for 10ns, Arduino can only go as low as 3us
		delayMicroseconds(5);
//		digitalWrite(_pinReset, LOW);
		if(_pinPower != SI4735_PIN_POWER_HW) digitalWrite(_pinPower, LOW);
	};
}

void Si4737::begin(byte mode, bool xosc, bool slowshifter){

	//RIC - Removing all irrelevant SPI stuff
/*
	//Reset pinout config
	pinMode(_pinReset, OUTPUT);
	//pinMode((_i2caddr ? SCL : SCK), OUTPUT); Dont know what this does
	delayMicroseconds(100);
	digitalWrite(_pinReset, LOW); //Reset it and wait
	//Use the longest of delays given in the datasheet
	delayMicroseconds(100);
	/* No idea
	if(_pinPower != SI4735_PIN_POWER_HW) {
	digitalWrite(_pinPower, HIGH);
	//Datasheet calls for 250us between VIO and RESET
	delayMicroseconds(250);
	};
	digitalWrite((_i2caddr ? SCL : SCK), LOW);
	*/
/*
	//Datasheet calls for no rising SCLK edge 300ns before RESET rising edge,
	//but Arduino can only go as low as 3us.
	delayMicroseconds(5);
	digitalWrite(_pinReset, HIGH);
	//Datasheet calls for 30ns from rising edge of RESET until GPO1/GPO2 bus
	//mode selection completes, but Arduino can only go as low as 3us.
*/
	delayMicroseconds(5);


	//Configure the I2C hardware
	Wire.begin();

	setMode(mode, false, xosc); //not used yet
}

void Si4737::authenticate()
{
  sendCommand(SI4735_CMD_GET_REV);
  byte response[16];
  getResponse(response);
  if (response[1] != _partNumberLastTwo)
  {
    SerialUSB.print("Si47xx::authenticate() ERROR: Part number from GET_REV [Si47");
    SerialUSB.print(response[1]);
    SerialUSB.print("] does not match expected part number [Si47");
    SerialUSB.print(_partNumberLastTwo);
    SerialUSB.println("]");
    while (true) { SerialUSB.print("."); delay(1000); }
  }
}

void Si4737::sendCommand(byte command, byte arg1, byte arg2, byte arg3,
						 byte arg4, byte arg5, byte arg6, byte arg7){
/*
							 SerialUSB.print("Si4735 CMD 0x");
							 SerialUSB.print(command, HEX);
							 SerialUSB.print(" (0x");
							 SerialUSB.print(arg1, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg1, BIN);
							 SerialUSB.print("], 0x");
							 SerialUSB.print(arg2, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg2, BIN);
							 SerialUSB.print("], 0x");
							 SerialUSB.print(arg3, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg3, BIN);
							 SerialUSB.println("],");
							 SerialUSB.print("0x");
							 SerialUSB.print(arg4, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg4, BIN);
							 SerialUSB.print("], 0x");
							 SerialUSB.print(arg5, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg5, BIN);
							 SerialUSB.print("], 0x");
							 SerialUSB.print(arg6, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg6, BIN);
							 SerialUSB.print("], 0x");
							 SerialUSB.print(arg7, HEX);
							 SerialUSB.print(" [");
							 SerialUSB.print(arg7, BIN);
							 SerialUSB.println("])");
							 SerialUSB.flush();
*/

							 Wire.beginTransmission(_i2caddr);
							 Wire.write(command);
							 Wire.write(arg1);
							 Wire.write(arg2);
							 Wire.write(arg3);
							 Wire.write(arg4);
							 Wire.write(arg5);
							 Wire.write(arg6);
							 Wire.write(arg7);
							 Wire.endTransmission();
							 byte status;
							 do {
								 status = getStatus();
							 } 
							 while(!(status & SI4735_STATUS_CTS));
}

bool Si4737::getRDSStat(){
	//See if there's anything for us to do
	if(!(_mode == SI4735_MODE_FM && (getStatus() & SI4735_STATUS_RDSINT)))
		_haverds = false;

	else
	{
		_haverds = true;
	}

	return _haverds;
}

bool Si4737::readRDSBlock(word* block){
	//See if there's anything for us to do
	if(!(_mode == SI4735_MODE_FM && (getStatus() & SI4735_STATUS_RDSINT)))
		return false;

	_haverds = true;
	//Grab the next available RDS group from the chip
	sendCommand(SI4735_CMD_FM_RDS_STATUS, SI4735_FLG_INTACK);
	getResponse(_response);
	//memcpy() would be faster but it won't help since we're of a different
	//endianness than the device we're talking to.
	block[0] = word(_response[4], _response[5]);
	block[1] = word(_response[6], _response[7]);
	block[2] = word(_response[8], _response[9]);
	block[3] = word(_response[10], _response[11]);

	return true;
}

void Si4737::getRSQ(Si4737_RX_Metrics* RSQ){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_RSQ_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_RSQ_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_WB:
		sendCommand(SI4735_CMD_WB_RSQ_STATUS, SI4735_FLG_INTACK);
		break;
	}
	//Now read the response
	getResponse(_response);

	//Pull the response data into their respecive fields
	RSQ->RSSI = _response[4];
	RSQ->SNR = _response[5];
	if(_mode == SI4735_MODE_FM){
		RSQ->PILOT = _response[3] & SI4735_STATUS_PILOT;
		RSQ->STBLEND = (_response[3] & (~SI4735_STATUS_PILOT));
		RSQ->MULT = _response[6];
		RSQ->FREQOFF = _response[7];
	}
}

void Si4737::getResponse(byte* response){

	Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)16);
	for(int i = 0; i < 16; i++) {
		//I2C runs at 100kHz when using the Wire library, 100kHz = 10us
		//period so wait 10 bit-times for something to become available.
		while(!Wire.available()) delayMicroseconds(100);
		response[i] = Wire.read();
	}

/*
	SerialUSB.print("Si4735 RSP");
	for(int i = 0; i < 4; i++) {
		if(i) SerialUSB.print("           ");
		else SerialUSB.print(" ");
		for(int j = 0; j < 4; j++) {
			SerialUSB.print("0x");
			SerialUSB.print(response[i * 4 + j], HEX);
			SerialUSB.print(" [");
			SerialUSB.print(response[i * 4 + j], BIN);
			SerialUSB.print("]");
			if(j != 3) SerialUSB.print(", ");
			else
				if(i != 3) SerialUSB.print(",");
		}
		SerialUSB.println("");
	}
*/
}

byte Si4737::getStatus(void){
	byte response = 0;
	//RIC DEBUG
	Wire.requestFrom((uint8_t)_i2caddr, (uint8_t)1);
	//I2C runs at 100kHz when using the Wire library, 100kHz = 10us period
	//so wait 10 bit-times for something to become available.
	while(!Wire.available()) delayMicroseconds(100);
	response = Wire.read();

//	SerialUSB.print(" gs");
//	SerialUSB.print(response);
/*
	if(response > 0){
		SerialUSB.print(F("\nSi4735 STS 0x"));
		SerialUSB.print(response, HEX);
		//SerialUSB.print(" [");
		//SerialUSB.print(response, BIN);
		//SerialUSB.println("]");
		//Trimming end of status byte
		if((response / 128) & 1) SerialUSB.print(" CTS");
		if((response / 64) & 1) SerialUSB.print(" ERR");
		if((response / 8) & 1) SerialUSB.print(" RSQ");
		if((response / 4) & 1) SerialUSB.print(" RDS");
		if((response / 2) & 1) SerialUSB.print(" ASQ");
		if(response & 1) SerialUSB.print(" STC");
		SerialUSB.print("\n\n");
	}
*/

	return response;
}

word Si4737::getProperty(word property){
	sendCommand(SI4735_CMD_GET_PROPERTY, 0x00, highByte(property),
		lowByte(property));
	getResponse(_response);

	return word(_response[2], _response[3]);
}

word Si4737::getFrequency(bool* valid){
	word frequency;

	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_STATUS);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_STATUS);
		break;
	case SI4735_MODE_WB:
		sendCommand(SI4735_CMD_WB_TUNE_STATUS);
		break;
	}
	getResponse(_response);
	frequency = word(_response[2], _response[3]);

	if(valid) *valid = (_response[1] & SI4735_STATUS_VALID);
	return frequency;
}

void Si4737::setProperty(word property, word value){
	sendCommand(SI4735_CMD_SET_PROPERTY, 0x00, highByte(property),
		lowByte(property), highByte(value), lowByte(value));
	//Datasheet states SET_PROPERTY completes 10ms after sending the command
	//irrespective of CTS coming up earlier than that
	delay(10);
}

void Si4737::setFrequency(long frequency){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency), 0x00,
			(0x00));
		break;
	case SI4735_MODE_WB: //Input should be NOAA channel no. (with no. 1 = 162.400 & no. 7 = 162.550)
		frequency = 64960 + (10 * (frequency - 1));
		sendCommand(SI4735_CMD_WB_TUNE_FREQ, 0x00, highByte(frequency),
			lowByte(frequency), 0x00,
			(0x00));
		break;
	}
	completeTune();
}

char Si4737::getMode()
{
  switch (_mode)
  {
    case SI4735_MODE_FM:
      return 'F';
    case SI4735_MODE_AM:
      return 'A';
    case SI4735_MODE_WB:
      return 'W';
    default:
      return 'U';
  }
}

void Si4737::setMode(byte mode, bool powerdown, bool xosc){
	if(powerdown) end(false);
	_mode = mode;

return;

	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_POWER_UP,
			SI4735_FUNC_FM,
			SI4735_OUT_ANALOG);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_POWER_UP,
			((_pinGPO2 == SI4735_PIN_GPO2_HW) ? 0x00 :
			SI4735_FLG_GPO2IEN) |
			(xosc ? SI4735_FLG_XOSCEN : 0x00) | SI4735_FUNC_AM,
			SI4735_OUT_ANALOG);
		break;
	case SI4735_MODE_WB: //Yes, the variable is wrong. Eventually I'll switch everything over to SI4737
		sendCommand(SI4735_CMD_POWER_UP, SI4735_FUNC_WB, SI4735_OUT_ANALOG);
		break;
	}

	//Configure GPO lines to maximize stability (see datasheet for discussion)
	//No need to do anything for GPO1 if using SPI
	//No need to do anything for GPO2 if using interrupts
	//	sendCommand(SI4735_CMD_GPIO_CTL, (_i2caddr ? SI4735_FLG_GPO1OEN : 0x00) |
	//		((_pinGPO2 == SI4735_PIN_GPO2_HW) ?
	//SI4735_FLG_GPO2OEN : 0x00));
	//Set GPO2 high if using interrupts as Si4735 has a LOW active INT line
	//if(_pinGPO2 != SI4735_PIN_GPO2_HW)
	//	sendCommand(SI4735_CMD_GPIO_SET, SI4735_FLG_GPO2LEVEL);

	//Enable end-of-seek and RDS interrupts, if we're actually using interrupts
	//TODO: write interrupt handlers for STCINT and RDSINT
	//if(_pinGPO2 != SI4735_PIN_GPO2_HW)
	//	setProperty(
	//	SI4735_PROP_GPO_IEN,
	//	word(0x00, ((_mode == SI4735_MODE_FM) ? SI4735_FLG_RDSIEN : 0x00) |
	//	SI4735_FLG_STCIEN));
}

void Si4737::seekUp(bool wrap){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_SEEK_START,
			(SI4735_FLG_SEEKUP |
			(wrap ? SI4735_FLG_WRAP : 0x00)));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_SEEK_START,
			(SI4735_FLG_SEEKUP | (wrap ? SI4735_FLG_WRAP : 0x00)),
			0x00, 0x00, 0x00, 0x00);
		break;
		//Sadly there is no seek for WB per AN332
	}
	completeTune();
}

void Si4737::seekDown(bool wrap){
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_SEEK_START,
			(wrap ? SI4735_FLG_WRAP : 0x00));
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_SEEK_START,
			(wrap ? SI4735_FLG_WRAP : 0x00), 0x00, 0x00, 0x00, 0x00);
		break;
	}
	completeTune();
}

void Si4737::enableRDS(void){
	//Enable and configure RDS reception
	if(_mode == SI4735_MODE_FM) {
		setProperty(SI4735_PROP_FM_RDS_INT_SOURCE, word(0x00,
			SI4735_FLG_RDSRECV));
		setProperty(SI4735_PROP_FM_RDS_INT_FIFO_COUNT, word(0x00, 0x04));
		setProperty(SI4735_PROP_FM_RDS_CONFIG, word(SI4735_FLG_BLETHA_35 |
			SI4735_FLG_BLETHB_35 | SI4735_FLG_BLETHC_35 |
			SI4735_FLG_BLETHD_35, SI4735_FLG_RDSEN));
	};
}

void Si4737::waitForInterrupt(byte which){
	while(!(getStatus() > 0 & which)){
		//Balance being snappy with hogging the chip
		delay(125);
		sendCommand(SI4735_CMD_GET_INT_STATUS);
	}
}

void Si4737::completeTune(void) {
	waitForInterrupt(SI4735_STATUS_STCINT);
	//Make future off-to-on STCINT transitions visible
	switch(_mode){
	case SI4735_MODE_FM:
		sendCommand(SI4735_CMD_FM_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_AM:
		sendCommand(SI4735_CMD_AM_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	case SI4735_MODE_WB:
		sendCommand(SI4735_CMD_WB_TUNE_STATUS, SI4735_FLG_INTACK);
		break;
	}

	if(_mode == SI4735_MODE_FM) enableRDS();
}

void Si4737::setDeemphasis(byte deemph){
    switch(_mode){
        case SI4735_MODE_FM:
            setProperty(SI4735_PROP_FM_DEEMPHASIS, word(0x00, deemph));
            break;
        // case SI4735_MODE_AM:
        // case SI4735_MODE_LW:
        // case SI4735_MODE_SW:
        //     setProperty(SI4735_PROP_AM_DEEMPHASIS, word(0x00, deemph));
        //     break;
    }
}

void Si4737::setAudioModeStereo(bool isStereo){
    switch(_mode){
        case SI4735_MODE_FM:
            setProperty( SI4735_PROP_FM_BLEND_STEREO_THRESHOLD, word(0x00, (isStereo ? SI4735_DEFAULT_FM_BLEND_STEREO_THRESHOLD : 127)));
            setProperty( SI4735_PROP_FM_BLEND_MONO_THRESHOLD, word(0x00, (isStereo ? SI4735_DEFAULT_FM_BLEND_MONO_THRESHOLD : 127)));

            setProperty( SI4735_PROP_FM_BLEND_RSSI_STEREO_THRESHOLD, word(0x00, (isStereo ? SI4735_DEFAULT_FM_BLEND_RSSI_STEREO_THRESHOLD : 127)));
            setProperty( SI4735_PROP_FM_BLEND_RSSI_MONO_THRESHOLD, word(0x00, (isStereo ? SI4735_DEFAULT_FM_BLEND_RSSI_MONO_THRESHOLD : 127)));

            // Per AN332.pdf
            setProperty( SI4735_PROP_FM_BLEND_SNR_STEREO_THRESHOLD, word(0x00, 0));
            setProperty( SI4735_PROP_FM_BLEND_SNR_MONO_THRESHOLD, word(0x00, 0));
            setProperty( SI4735_PROP_FM_BLEND_MULTIPATH_STEREO_THRESHOLD, word(0x00, 100));
            setProperty( SI4735_PROP_FM_BLEND_MULTIPATH_MONO_THRESHOLD, word(0x00, 100));

            break;
    }
}
