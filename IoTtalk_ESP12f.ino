#define DM_name  "ESP12F" 
#define DF_list  {"ESP12F_IDF", "ESP12F_ODF", "ESP12F_LED"}
#define nODF     10  // The max number of ODFs which the DA can pull.
#include "MyEsp8266.h"

extern char IoTtalkServerIP[100];
HTTPClient http;
String url = "";
String df_name_list[nODF];
String df_timestamp[nODF];
long cycleTimestamp = millis();
String result;


int iottalk_register(void){
    url = "http://" + String(IoTtalkServerIP) + ":9999/";  
    
    String df_list[] = DF_list;
    int n_of_DF = sizeof(df_list)/sizeof(df_list[0]); // the number of DFs in the DF_list
    String DFlist = ""; 
    for (int i=0; i<n_of_DF; i++){
        DFlist += "\"" + df_list[i] + "\"";  
        if (i<n_of_DF-1) DFlist += ",";
    }
  
    uint8_t MAC_array[6];
    WiFi.macAddress(MAC_array);//get esp12f mac address
    for (int i=0;i<6;i++){
        if( MAC_array[i]<0x10 ) url+="0";
        url+= String(MAC_array[i],HEX);;      //Append the mac address to url string
    }
 
    //send the register packet
    Serial.println("[HTTP] POST..." + url);
    String profile="{\"profile\": {\"d_name\": \"";
    profile += DM_name;
    profile += "." + String(MAC_array[4],HEX) + String(MAC_array[5],HEX);
    profile += "\", \"dm_name\": \"";
    profile += DM_name;
    profile += "\", \"is_sim\": false, \"df_list\": [";
    profile +=  DFlist;
    profile += "]}}";

    http.begin(url);
    http.addHeader("Content-Type","application/json");
    int httpCode = http.POST(profile);

    Serial.println("[HTTP] Register... code: " + (String)httpCode );
    Serial.println(http.getString());
    //http.end();
    url +="/";  
    return httpCode;
}

void init_ODFtimestamp(){
  for (int i=0; i<=nODF; i++) 
    df_timestamp[i] = "";
  
  for (int i=0; i<=nODF; i++) 
    df_name_list[i] = "";  
}

int DFindex(char *df_name){
    for (int i=0; i<=nODF; i++){
        if (String(df_name) ==  df_name_list[i]) return i;
        else if (df_name_list[i] == ""){
            df_name_list[i] = String(df_name);
            return i;
        }
    }
    return nODF+1;  // df_timestamp is full
}

int push(char *df_name, String value){
    http.begin( url + String(df_name));
    http.addHeader("Content-Type","application/json");
    String data = "{\"data\":[" + value + "]}";
    int httpCode = http.PUT(data);
    if (httpCode != 200) Serial.println("[HTTP] PUSH \"" + String(df_name) + "\"... code: " + (String)httpCode + ", retry to register.");
    while (httpCode != 200){
        digitalWrite(LEDPIN, HIGH);
        httpCode = iottalk_register();
        if (httpCode == 200)  http.PUT(data);
        else delay(3000);
    }    
    return httpCode;
}

String pull(char *df_name){
  DynamicJsonBuffer jsonBuffer;

  http.begin( url + String(df_name) );
  http.addHeader("Content-Type","application/json");
  int httpCode = http.GET(); //http state code
  if (httpCode != 200) {
    Serial.println("[HTTP] PULL \"" + String(df_name) + "\"... code: " + (String)httpCode + ", retry to register.");
  }
  
  while (httpCode != 200){
      digitalWrite(LEDPIN, HIGH);
      httpCode = iottalk_register();
      if (httpCode == 200) http.GET();
      else delay(3000);
  }
  String get_ret_str = http.getString();  //After send GET request , store the return string
  JsonObject& root = jsonBuffer.parseObject(get_ret_str);
  //Serial.println(get_ret_str);
  http.end();

  String portion = "";  //This portion is used to fetch the timestamp.
  if (get_ret_str.indexOf("samples") >=0){ // if not found the string , it will return -1
      portion = root["samples"][0][0].as<String>();
      
      if (df_timestamp[DFindex(df_name)] != portion){
          df_timestamp[DFindex(df_name)] = portion;
          portion = root["samples"][0][1].as<String>();
          portion[0] = ' ';
          portion[portion.length()-1] = 0;
          
          return portion;   // return the data.
       }
       else return "___NULL_DATA___";
  }
  else return "___NULL_DATA___";
}

void setup() {
    pinMode(CLEAREEPROM, INPUT_PULLUP); //GPIO13: clear eeprom button
    pinMode(LEDPIN, OUTPUT);//GPIO2 : on board led
    digitalWrite(LEDPIN,HIGH);
    pinMode(16, OUTPUT);//GPIO16 : relay signal
    digitalWrite(16,LOW);

    EEPROM.begin(512);
    Serial.begin(115200);

    char wifissid[100]="";
    char wifipass[100]="";
    int statesCode = read_WiFi_AP_Info(wifissid, wifipass, IoTtalkServerIP);
    //for (int k=0; k<50; k++) Serial.printf("%c", EEPROM.read(k) );  //inspect EEPROM data for the debug purpose.
  
    if (!statesCode){  
      connect_to_wifi(wifissid, wifipass);
    }
    else{
        Serial.println("Laod setting failed! statesCode: " + String(statesCode)); // StatesCode 1=No data, 2=ServerIP with wrong format
        ap_setting();
    }
    //while(wifimode) server.handleClient(); //waitting for connecting to AP ;

    statesCode = 0;
    while (statesCode != 200) {
        statesCode = iottalk_register();
        if (statesCode != 200){
            Serial.println("Retry to register to the IoTtalk server. Suspend 3 seconds.");
            if (digitalRead(CLEAREEPROM) == LOW) clr_eeprom(0);
            delay(3000);
        }
    }
    init_ODFtimestamp();
}

void loop() {
    if (digitalRead(CLEAREEPROM) == LOW){
        clr_eeprom(0);
    }
 
    if (millis() - cycleTimestamp > 500) {
        /*
        result = pull("ESP12F_LED");
        if (result != "___NULL_DATA___"){
          if (result.toInt() > 0) {
            digitalWrite(LEDPIN,LOW);
          }
          else if(result.toInt() == 0) {
            digitalWrite(LEDPIN,HIGH);
          }
        }*/
        push("ESP12F_IDF", String(millis()));        
    
        cycleTimestamp = millis();
    }

}
