#include <ArduinoJson.h>
#include "LogFormatter.h"
#include "mystrlib.h"
#include "DataLogger.h"
#include "Config.h"
#include "TemperatureFormats.h"
#include "BrewPiProxy.h"
#include "ExternalData.h"
#include "BPLSettings.h"
#if SupportPressureTransducer
#include "PressureMonitor.h"
#endif
#if EnableHumidityControlSupport
#include "HumidityControl.h"
#endif
extern BrewPiProxy brewPi;



static char modeInInteger(char mode){
	char modevalue;
	if(mode == 'p') modevalue = '3';
	else if(mode == 'b') modevalue = '2';
	else if(mode == 'f') modevalue = '1';
	else modevalue = '0';
	return modevalue;
}

const char* stateDescription(uint8_t state){
	static const char* descriptions[] = {
		"idle", "off", "doorOpen", "heating", "cooling", "waitingToCool",
		"waitingToHeat", "waitingForPeak", "coolingMinimumTime", "heatingMinimumTime"
	};
	return state < sizeof(descriptions) / sizeof(descriptions[0]) ? descriptions[state] : "invalid";
}

size_t printFloat(char* buffer,float value,int precision,bool valid,const char* invalidstr)
{
	if(valid){
		return sprintFloat(buffer,value,precision);
	}else{
        strcpy(buffer,invalidstr);
        return strlen(invalidstr);
	}
}

size_t dataSprintf(char *buffer,size_t size,const char *format,const char* invalid)
{
	if(!buffer || size == 0 || !format || !invalid) return 0;
	int i=0;
	size_t d=0;
	char value[48];
	auto append = [&](const char* source, size_t length) -> bool {
		if(length >= size - d) return false;
		memcpy(buffer + d, source, length);
		d += length;
		return true;
	};
	for(i=0;i< (int) strlen(format);i++){
		char ch=format[i];
		if( ch == '%'){
			if(format[i + 1] == '\0') return 0;
			i++;
			ch=format[i];
			if(ch == '%'){
				value[0]=ch;
				if(!append(value,1)) return 0;
			}else if(ch == 'b'){
				float  beerTemp = brewPi.getBeerTemp();
				size_t length = printFloat(value,beerTemp,1,IS_FLOAT_TEMP_VALID(beerTemp),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'B'){
				float  beerSet = brewPi.getBeerSet();
				size_t length = printFloat(value,beerSet,1,IS_FLOAT_TEMP_VALID(beerSet),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'f'){
				float fridgeTemp = brewPi.getFridgeTemp();
				size_t length = printFloat(value,fridgeTemp,1,IS_FLOAT_TEMP_VALID(fridgeTemp),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'F'){
				float fridgeSet = brewPi.getFridgeSet();
				size_t length = printFloat(value,fridgeSet,1,IS_FLOAT_TEMP_VALID(fridgeSet),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'r'){
				float  roomTemp = brewPi.getRoomTemp();
				size_t length = printFloat(value,roomTemp,1,IS_FLOAT_TEMP_VALID(roomTemp),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'g'){
				float sg=externalData.gravity();
				size_t length = printFloat(value,sg,4,IsGravityValid(sg),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'p'){
				float sg=externalData.plato();
				size_t length = printFloat(value,sg,2,IsGravityValid(sg),invalid);
				if(!append(value,length)) return 0;
			}
			else if(ch == 'P'){
			#if SupportPressureTransducer
				size_t length = printFloat(value,PressureMonitor.currentPsi(),1,PressureMonitor.isCurrentPsiValid(),invalid);
			#else
				size_t length = strlen(invalid);
				memcpy(value,invalid,length);
			#endif
				if(!append(value,length)) return 0;
			}
			else if(ch == 'v'){
				float vol=externalData.deviceVoltage();
				size_t length = printFloat(value,vol,1,IsVoltageValid(vol),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'a'){
				float at=externalData.auxTemp();
				size_t length = printFloat(value,at,1,IS_FLOAT_TEMP_VALID(at),invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 't'){
				float tilt=externalData.tiltValue();
				size_t length = printFloat(value,tilt,2,true,invalid);
				if(!append(value,length)) return 0;
			}else if(ch == 'u'){
				size_t length = sprintInt(value, externalData.lastUpdate());
				if(!append(value,length)) return 0;
			}else if(ch == 'U'){
				value[0] = brewPi.getUnit();
				if(!append(value,1)) return 0;
			}else if(ch == 'm'){
				value[0] = modeInInteger(brewPi.getMode());
				if(!append(value,1)) return 0;
			}else if(ch == 'M'){
				value[0] = brewPi.getMode();
				if(!append(value,1)) return 0;
			}else if(ch == 's'){
				value[0] = '0' + brewPi.getState();
				if(!append(value,1)) return 0;
			}else if(ch == 'H'){
				const char* hostname = theSettings.systemConfiguration()->hostnetworkname;
				if(!append(hostname,strlen(hostname))) return 0;
			}else if(ch == 'h'){
				#if EnableHumidityControlSupport
				size_t length = printFloat(value,(float)humidityControl.humidity(),0,humidityControl.isHumidityValid(),invalid);
				#else
				size_t length = strlen(invalid);
				memcpy(value,invalid,length);
				#endif
				if(!append(value,length)) return 0;
			}else if(ch == 'E'){
				#if EnableHumidityControlSupport
				size_t length = printFloat(value,(float)humidityControl.roomHumidity(),0,humidityControl.isRoomSensorInstalled(),invalid);
				#else
				size_t length = strlen(invalid);
				memcpy(value,invalid,length);
				#endif
				if(!append(value,length)) return 0;
			}else{				
				// wrong format
				//return 0; ignored
			}
		}else{
			value[0]=ch;
			if(!append(value,1)) return 0;
		}
	}// for each char

	buffer[d]='\0';
	return d;
}

/*
int _copyName(char *buf,char *name,bool concate)
{
	char *ptr=buf;
	if(name ==NULL) return 0;
	if(concate){
		*ptr='&';
		ptr++;
	}
	int len=strlen(name);
	strcpy(ptr,name);
	ptr+=len;
	*ptr = '=';
	ptr++;
	return (ptr - buf);
}

int copyTemp(char* buf,char* name,float value, bool concate)
{
	int n;
	if((n = _copyName(buf,name,concate))!=0){
		if(IS_FLOAT_TEMP_VALID(value)){
			n += sprintFloat(buf + n ,value,2);
		}else{
			strcpy(buf + n,"null");
			n += 4;
		}

	}
	return n;
}
*/

size_t nonNullJson(char* buffer,size_t size)
{
	const int JSON_BUFFER_SIZE = JSON_OBJECT_SIZE(16);
	
	#if ARDUINOJSON_VERSION_MAJOR == 6
	DynamicJsonDocument root(JSON_BUFFER_SIZE +size);
	#else

	DynamicJsonBuffer jsonBuffer(JSON_BUFFER_SIZE);
	JsonObject& root = jsonBuffer.createObject();
	#endif

	uint8_t state, mode;
	float beerSet,fridgeSet;
	float beerTemp,fridgeTemp,roomTemp;

    state = brewPi.getState();
    mode = brewPi.getMode();
    beerTemp = brewPi.getBeerTemp();
    beerSet = brewPi.getBeerSet();
    fridgeTemp = brewPi.getFridgeTemp();
    fridgeSet = brewPi.getFridgeSet();
    roomTemp = brewPi.getRoomTemp();


	root[KeyState] = state;
	root[KeyStateDescription] = stateDescription(state);

	if(IS_FLOAT_TEMP_VALID(beerTemp)) root[KeyBeerTemp] = beerTemp;
	if(IS_FLOAT_TEMP_VALID(beerSet)) root[KeyBeerSet] = beerSet;
	if(IS_FLOAT_TEMP_VALID(fridgeTemp)) root[KeyFridgeTemp] = fridgeTemp;
	if(IS_FLOAT_TEMP_VALID(fridgeSet)) root[KeyFridgeSet] = fridgeSet;
	if(IS_FLOAT_TEMP_VALID(roomTemp)) root[KeyRoomTemp] = roomTemp;

	root[KeyMode] =(int)( modeInInteger(mode) - '0');
	#if SupportPressureTransducer
	if(PressureMonitor.isCurrentPsiValid()) root[KeyPressure]= PressureMonitor.currentPsi();
	#endif

	#if EnableHumidityControlSupport
	if(humidityControl.isHumidityValid()) root[KeyFridgeHumidity] = humidityControl.humidity();
	if(humidityControl.isRoomSensorInstalled()){
		uint8_t rh = humidityControl.roomHumidity();
		if(rh<100) root[KeyRoomHumidity] =rh;
	}
	#endif

	float sg=externalData.gravity();
	if(IsGravityValid(sg)){
		root[KeyGravity] = sg;
		root[KeyPlato] = externalData.plato();
	}

	// Hydrometer data
	float vol=externalData.deviceVoltage();
	if(IsVoltageValid(vol)) root[KeyVoltage] = vol;
	float at=externalData.auxTemp();
	if(IS_FLOAT_TEMP_VALID(at)) root[KeyAuxTemp] = at;
	
	float tilt=externalData.tiltValue();
	if(tilt >0)	root[KeyTilt]=tilt;
	
	int16_t rssi=externalData.rssi();
	if(IsRssiValid(rssi)) root[KeyWirelessHydrometerRssi]=rssi;
	const char *hname=externalData.getDeviceName();
	if(hname) root[KeyWirelessHydrometerName] = hname;

	#if ARDUINOJSON_VERSION_MAJOR == 6
	return	serializeJson(root,buffer,size);
	#else
	return root.printTo(buffer,size);
	#endif
}
