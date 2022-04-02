#include <Arduino.h> 
#include "config_check-mate.h" 
#include <WiFi.h> 
#include <HTTPClient.h> 
#include <Arduino_JSON.h> 
#include <LinkedList.h> 
#include <AsyncTCP.h> 
#include <AsyncHTTPSRequest_Generic.h>  
#include <AsyncHTTPSRequest_Impl_Generic.h> 
#include <Adafruit_MCP23x17.h>   
// Level from 0-4 
#define ASYNC_HTTP_DEBUG_PORT Serial 
#define _ASYNC_HTTP_LOGLEVEL_ 7 
TaskHandle_t TaskPositionDetection;
int lastPositionDetection=0;
int PositionDetectionDelay=25;
String lastpressedbutton="";
int volatile towritetmp[8][8];
AsyncHTTPSRequest request; 
String serverName = "https://lichess.org/api";   // URL der Server API 
JSONVar lastRequestText; 
String runningRequest = ""; 
bool finishedRequest=false; 
String currentPlayingState = "";          // Der Status des Benutzers 
String currentGameId = "";             // Die Kennzeichnungsnummer der laufenden Schachpartie 
String currentMoveToPlay = "undefined";       // Die Seite (Farbe), welche gerade am Zug ist 
String userid = "";                 // Die Kennzeichnungsnummer des Benutzers 
String currentUserSide = "";            // Die Seite (Farbe) des Benutzers 
bool isTimed = false;                // zeigt, ob eine Schachpartie auf Zeit gespielt wird 
int whiteTime= undefined;             // Zeit des Spielers der weissen Seite 
int blackTime = undefined;             // Zeit des Spielers der schwarzen Seite 
String playingFormat = ""; 
String moveToTransmit = "";   
String sentMove="";
LinkedList<String> illegalMoves;   
String enemyMove=""; 
int testcount=0;  // Zum Testen
boolean waitingForEnemyMove=true;      
unsigned long lastTimeSendingRequest = 0;       // Zeitpunkt der letzten Abfrage 
unsigned long timerDelaySendingRequest = 3000;    // Verzoegerung fuer die Abfrage
int PINA0 = 5;
int PINA1 = 4;
int PINA2 = 15;
int LED1 = 13;
int LED2 = 12;
int LED3 = 14;
int LED4 = 27;
int LED5 = 26;
int LED6 = 25;
int LED7 = 33;
int LED8 = 32;
unsigned int T = 1000000/50; // Periode in Mikrosekunden
LinkedList<int> LEDList[8];
unsigned long lastLEDSwitch =10000;
int currentPeriodePart=0;
hw_timer_t * timer = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
int count = 0;
Adafruit_MCP23X17 mcp;   
int counter=0;  
unsigned int TPositionDetection=1000;  
unsigned int timeofPeriod=0;  
volatile unsigned int timePressed=0; 
unsigned int delaying = 100; 
String unfinishedPress = "";
String unfinishedMove = "";
String moveToPlay="";
volatile bool triggeredInterrupt=false; 
volatile bool stop = false;
int INTAPin =18; 
int TasterSteuerungsPin2 = 23;
int TasterSteuerungsPin5 = 19;
int pinLauefer = 35;
int pinSpringer = 34;
int pinDame = 36; 
unsigned int lastInterruptTime=0;
int interruptDelay=30;
int figur=-1;
volatile int activeButton =5;
int lastButtonTime=0;
int buttonDelay=3;
boolean buttonInterruptAttachement =true;
boolean starting =true;
String chessBoard[9][8];
String tmplastposition ="";
boolean resetMCP=true;
unsigned long positionDetectionPressDelay=0;
volatile boolean deleteNextDetectedPress=false;
void markPosition(String position, boolean state, boolean blink);
int letterToNumber (String letter);
void setLEDLow (int number);
void set1To8High();
void IRAM_ATTR UpdateLEDMatrix();
void LEDMatrixSetup();
void lookForPlayingState(); 
void getGameStatus(); 
void chessMoveResponseHandling();
void handleCorrectionMove();
void sendRequest(String requestPath, String method, String requestName); 
void requestCB(void *optParm, AsyncHTTPSRequest *request, int readyState); 
String calculateWhoPlaysNextMove(JSONVar); 
LinkedList<String> splitString(String, String);
String numberToLetter(int number); 
void IRAM_ATTR fieldPressed();
void positionDetectionSetup();
void positionDetectionloop(void * pvParameters);
void refreshLEDMatrix();
void resetAllLEDs();
void IRAM_ATTR laueferInterrupt();
void IRAM_ATTR springerInterrupt();
void IRAM_ATTR dameInterrupt();
void detachButtons();
void initChessBoard();
void printChessBoard();
void updateChessBoard(String moveToUpdate);
void castlingHandling(String moveToCheck);
void IRAM_ATTR test(){
 portENTER_CRITICAL_ISR(&timerMux);
 Serial.println(micros());
 portEXIT_CRITICAL_ISR(&timerMux);
}
void setup() {
 Serial.begin(215200);        // Starten der seriellen Schnittstelle 
 
 initChessBoard();
 //Taster Setup
 pinMode(TasterSteuerungsPin2,OUTPUT);
 pinMode(TasterSteuerungsPin5,OUTPUT);
 pinMode(pinLauefer,INPUT);  
 pinMode(pinSpringer,INPUT);    
 pinMode(pinDame,INPUT);  
 //attachInterrupt(digitalPinToInterrupt(pinDame), dameInterrupt, FALLING); 
 attachInterrupt(digitalPinToInterrupt(pinLauefer), laueferInterrupt, FALLING); 
 attachInterrupt(digitalPinToInterrupt(pinSpringer), springerInterrupt, FALLING);  
 positionDetectionSetup();    
 WiFi.begin(ssid, password);    // Starten der WiFi Verbindung 
 Serial.println("Connecting");   
 // Abwarten des Verbindungsaufbaus 
 while(WiFi.status() != WL_CONNECTED) { 
  delay(500); 
  Serial.print("."); 
 } 
 Serial.println(""); 
 request.setDebug(false);  
 request.onReadyStateChange(requestCB);
 request.setTimeout(30); 
 while(userid.equals("")){ 
  if ((millis() - lastTimeSendingRequest) > timerDelaySendingRequest && runningRequest.equals("")) { 
   Serial.println("Abfragen des Spieler-Benutzerkontos."); 
   sendRequest("/account", "GET","LichessConnection"); 
   lastTimeSendingRequest = millis();   
  } 
  
  if(runningRequest.equals("LichessConnection") && finishedRequest){ 
    lastRequestText = JSON.parse(request.responseText()); 
   userid=lastRequestText["id"]; 
   runningRequest=""; 
   finishedRequest=false; 
   request.abort();
   
  } 
  
 }
 
 Serial.print("Spieler ID: "); 
 Serial.println(userid); 
 Serial.println(); 
 LEDMatrixSetup();
 request.setTimeout(120);
 xTaskCreatePinnedToCore(
          positionDetectionloop,  /* Task function. */
          "TaskPositionDetection",   /* name of task. */
          10000,    /* Stack size of task */
          NULL,    /* parameter of the task */
          2,      /* priority of the task */
          &TaskPositionDetection,   /* Task handle to keep track of created task */
          0);     /* pin task to core 0 */  
 
}
void loop() {
  refreshLEDMatrix();
 
 // Verarbeiten der Requests 
  if(finishedRequest){ 
    Serial.println("Interpreting Requests"); 
    lastRequestText = JSON.parse(request.responseText()); 
  if(runningRequest.equals("PlayerState")){ 
    lookForPlayingState(); 
    Serial.println("Spielerabfrage interpretation"); 
  }else if(runningRequest.equals("GameState")){ 
    if(request.responseHTTPcode()==-4){
      Serial.println("Restarting request");
    }else{
      Serial.println("Spielabfrage interpretation"); 
      getGameStatus(); 
    }
  }else if(runningRequest.equals("ChessMove")){ 
    chessMoveResponseHandling();
  } 
  Serial.print("RequestTime:");
  Serial.println(millis()-lastTimeSendingRequest);
  timerDelaySendingRequest=3000+((millis()-lastTimeSendingRequest)/3);
  lastTimeSendingRequest=millis();
  runningRequest = ""; 
  finishedRequest=false; 
  request.abort();
  
 } 
 // Senden der Requests 
 if(!finishedRequest && runningRequest.equals("")){ 
  if ((millis() - lastTimeSendingRequest) > timerDelaySendingRequest && currentPlayingState.equals("")) { 
   sendRequest("/stream/event","GET","PlayerState"); 
   lastTimeSendingRequest = millis();   
   Serial.println("Spielerstatus"); 
  } 
  // Ausfuehren eines Schachzuges, wenn der Benutzer am Zug ist 
 
  if(currentPlayingState.equals("gameStart") && currentUserSide.equals(currentMoveToPlay) && runningRequest.equals("") && (millis() - lastTimeSendingRequest) > timerDelaySendingRequest ){ 
  
   if(!moveToTransmit.equals("") && !waitingForEnemyMove){ 
    if(enemyMove.equals("") && illegalMoves.size()==0){
     // Für die Bauernumwandlung
     if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa") && (moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8) && moveToTransmit.length()<5){
      Serial.println("Warten auf Bauernumwandlung");
     }else{
      Serial.println("Übermitteln eines Schachzuges.");  
      sendRequest("/board/game/"+currentGameId+"/move/"+moveToTransmit, "POST","ChessMove"); 
      lastTimeSendingRequest=millis();
      sentMove=moveToTransmit;
      moveToTransmit=""; 
      Serial.println(); 
     }
     
    }else{
     if(enemyMove.length()==5 && moveToTransmit.length()<5){
      Serial.println("Enemy Move: Warten auf Bauernumwandlung");
     }else{
      handleCorrectionMove();
     }
    }
   }
  } 
  if ((millis() - lastTimeSendingRequest) > timerDelaySendingRequest ) { 
   // Abfragen des Spielstatuses, wenn ein Schachspiel gespielt wird 
   if(currentPlayingState.equals("gameStart")&& runningRequest.equals("")){ 
    sendRequest("/board/game/stream/"+currentGameId, "GET","GameState"); 
    Serial.println("Spielstatus"); 
    lastTimeSendingRequest = millis(); 
   }else{ 
    // Zuruecksetzen der Spielwerte 
    currentMoveToPlay = "undefined"; 
    currentUserSide = ""; 
    isTimed = false; 
    whiteTime = undefined; 
    blackTime = undefined; 
    playingFormat = ""; 
   } 
  } 
  
 } 
 
 
}
void sendRequest(String requestPath, String method, String requestName) 
{ 
 static bool requestOpenResult; 
 
 if (request.readyState() == readyStateUnsent || request.readyState() == readyStateDone) 
  {   
  requestOpenResult = request.open(method.c_str(), (serverName+requestPath).c_str()); 
  request.setReqHeader("Authorization", ("Bearer " + accessToken).c_str()); 
  if (requestOpenResult) 
  { 
  request.send(); 
  runningRequest = requestName; 
  } else { 
  Serial.println("Can't send bad request");  
 }  
 } 
 else   
 { 
 Serial.println("Can't send request");  
 } 
} 
void requestCB(void *optParm, AsyncHTTPSRequest *request, int readyState) 
{ 
(void)optParm; 
if (readyState == readyStateDone)  
 { 
   
  finishedRequest=true;  
 } 
} 
/* 
 Verwertung der Benutzerstatus-Abfrage 
*/ 
void lookForPlayingState(){ 
 Serial.println("Abfragen des Spielerstatuses."); 
 JSONVar response = lastRequestText; 
 currentPlayingState = response["type"];  // Auslesen des Benutzerstatuses 
 currentGameId = response["game"]["id"];  // Auslesen der Kennzeichnungsnummer der Schachpartie 
 Serial.print("Status des Spielers: "); 
 Serial.println(currentPlayingState); 
 Serial.print("ID der Schachpartie: "); 
 Serial.println(currentGameId); 
 Serial.println(); 
} 
/* 
 Verwertung der Spielstatus-Abfrage 
*/ 
void getGameStatus(){ 
 Serial.println("Abfragen der Schachpartie Informationen."); 
 if(request.responseHTTPcode()!=200){
   Serial.println(lastRequestText);
  Serial.println(request.responseHTTPcode());
 }
 JSONVar response = lastRequestText; 
 // Auswerten der Spieler Seite (Farbe) 
 if((String) (const char*)response["white"]["id"] == userid){ 
  currentUserSide = "white"; 
 }else if((String) (const char*)response["black"]["id"] == userid){ 
  currentUserSide = "black"; 
 }else { 
  currentPlayingState=""; 
  currentUserSide = "undefined"; 
  Serial.println("FEHLER:");
  Serial.println(response);
  Serial.println(request.responseHTTPcode());
  } 
 Serial.print("Farbe des Spielers: "); 
 Serial.println(currentUserSide);
 currentMoveToPlay = calculateWhoPlaysNextMove(response["state"]["moves"]);  // Auswerten, welche Seite am Zug (Farbe) ist 
 Serial.print("Farbe, welche am Zug ist: "); 
 Serial.println(currentMoveToPlay);  
 if(!currentMoveToPlay.equals(currentUserSide)){
  waitingForEnemyMove=true;
 }
 if(starting){
  LinkedList<String> moves = splitString((String)((const char*) response["state"]["moves"])," ");
  if(moves.size()>0){
   for (size_t i = 0; i < moves.size()-1; i++)
   {
    updateChessBoard(moves.get(i));
   }
  }
  printChessBoard();
  starting =false;
  moves.clear();
  
 }
 if(currentMoveToPlay.equals(currentUserSide) && waitingForEnemyMove){
  LinkedList<String> moves = splitString((String)((const char*) response["state"]["moves"])," ");
  enemyMove = moves.get(moves.size()-1);
  castlingHandling(enemyMove);
  waitingForEnemyMove=false;
  Serial.print("Enemy Move: ");
  Serial.println(enemyMove);
  moves.clear();
  
  
 }
 
 // Auswerten, ob die Schachpartie auf Zeit gespielt wird. 
 if(response["clock"] == null){ 
  isTimed=false; 
 }else { 
  isTimed=true; 
 } 
 Serial.print("Wird die Partie auf Zeit gespielt: "); 
 Serial.println(isTimed); 
 // Auswerten der Zeiten, falls die Partie auf Zeit gespielt wird 
 if(isTimed){ 
  whiteTime = response["state"]["wtime"]; 
  blackTime = response["state"]["btime"]; 
 } 
 // Abspeichern des Spielformates 
 playingFormat = response["speed"]; 
 Serial.print("Spielformat: "); 
 Serial.println(playingFormat);  
 String tmpgamestatus=""; 
 tmpgamestatus= response["state"]["status"]; 
 if(tmpgamestatus.equals("mate") || tmpgamestatus.equals("resign")){ 
  currentPlayingState=""; 
  illegalMoves.clear();
  enemyMove="";
  moveToTransmit="";
  Serial.println("Das Spiel wurde beendet."); 
 } 
 Serial.println(); 
} 
void chessMoveResponseHandling(){
 Serial.println("Interpretieren der Schachzug Durchführung"); 
 JSONVar response = lastRequestText;
 if(request.responseHTTPcode() == 400){
  Serial.println(response["error"]);
  if(!sentMove.equals("")){
   illegalMoves.add(sentMove);
  }
  Serial.print("illegaler Zug: ");
  Serial.println(sentMove);
 }else if(request.responseHTTPcode() == 200){
  castlingHandling(sentMove);
  updateChessBoard(sentMove);
  waitingForEnemyMove=true;
 }
sentMove="";
}
void handleCorrectionMove(){
 String reversedMoveToTransmit=moveToTransmit.substring(2,4)+ moveToTransmit.substring(0,2);
 if(illegalMoves.size()==0 && !enemyMove.equals("")){
  if(enemyMove.equals(moveToTransmit)){
   updateChessBoard(enemyMove);
   enemyMove="";
  }else{
   illegalMoves.add(moveToTransmit);
  }
 }else{
  
  if(illegalMoves.get(illegalMoves.size()-1).equals(reversedMoveToTransmit)){
   illegalMoves.remove(illegalMoves.size()-1);
  }else{
   illegalMoves.add(moveToTransmit);
  }
 }
 moveToTransmit="";
}
String calculateWhoPlaysNextMove(JSONVar moves){ 
 String tmp = (String) ( (const char*) moves);  // Umwandeln des JSON-Objekts in einen String 
 LinkedList<String> movesAsString = splitString(tmp," ");  // Zerlegen des Strings in die einzelnen Zuege 
 // Zurueckgeben der Seite (Farbe), welcher gerade am Zug ist 
 // ungerade = schwarze ; gerade = weiss 
 if(movesAsString.size() %2 == 0){ 
  movesAsString.clear();
  return "white"; 
 }else if(movesAsString.size() %2 == 1){ 
  movesAsString.clear();
  return "black"; 
 } 
 movesAsString.clear();
 return "undefined"; 
} 
/* 
 Aufteilen eines String durch einen Seperator 
*/ 
LinkedList<String> splitString(String inputString, String separator){ 
 LinkedList<int> listOfIndexes = LinkedList<int>();  // Liste der Seperatoren 
 int lastIndex = 0; 
 // Herausfiltern der Seperatoren 
 while(inputString.indexOf(" ",lastIndex+1) != -1){ 
  int index = inputString.indexOf(" ",lastIndex+1); 
  listOfIndexes.add(index); 
  lastIndex = index; 
 } 
 lastIndex=0; 
 LinkedList<String> parts = LinkedList<String>(); 
 // Zurueckgeben des ganzen Strings, wenn kein Seperator gefunden wurde 
 if(listOfIndexes.size() == 0){ 
  if(!inputString.equals("")){ 
   parts.add(inputString); 
  }
  listOfIndexes.clear(); 
  return parts; 
 } 
 // Abspeichern der einzelnen Stringteile 
 while(lastIndex <= listOfIndexes.size()){ 
  if(lastIndex==0){ 
   parts.add(inputString.substring(0, listOfIndexes.get(lastIndex)));    // Abspeichern des ersten Stringteils 
  }else if (lastIndex == listOfIndexes.size()){ 
   parts.add(inputString.substring( listOfIndexes.get(lastIndex-1) + 1));  // Abspeichern des letzen Stringteils 
  }else{ 
   parts.add(inputString.substring( listOfIndexes.get(lastIndex-1) + 1 , listOfIndexes.get(lastIndex)));  // Abspeichern der mittleren Stringteile 
  } 
  lastIndex++; 
 } 
 listOfIndexes.clear();
 return parts; 
} 
void LEDMatrixSetup(){
 
 pinMode(PINA0, OUTPUT);
 pinMode(PINA1, OUTPUT);
 pinMode(PINA2, OUTPUT);
 pinMode(LED1,OUTPUT);
 pinMode(LED2,OUTPUT);
 pinMode(LED3,OUTPUT);
 pinMode(LED4,OUTPUT);
 pinMode(LED5,OUTPUT);
 pinMode(LED6,OUTPUT);
 pinMode(LED7,OUTPUT);
 pinMode(LED8,OUTPUT);
 timer = timerBegin(0, 80, true);
 
 timerAttachInterrupt(timer, &UpdateLEDMatrix, true);
 timerAlarmWrite(timer, T/8, true);
 timerAlarmEnable(timer);
}
void markPosition(String position, boolean state, boolean blink){
 
 if(state){
  int blinkOffset=0;
  if(blink){
   blinkOffset=100;
  }
  LEDList[letterToNumber(position.substring(0,1))].add(position.substring(1,2).toInt()+blinkOffset); // erstes substring könnte gelöscht werden
  
 }else {
  int index = letterToNumber(position.substring(0,1));
  
  int searchingNumber = position.substring(1,2).toInt();
  
  for (int i = 0; i < LEDList[index].size(); i++)
  {
   if(LEDList[index].get(i)%100 == searchingNumber){ // %100 wegen dem blink offset
    LEDList[index].remove(i);
    i--;
   }
  }
  
 }
}
int letterToNumber (String letter){
 letter.toLowerCase();
 switch (letter.charAt(0))
 {
 case 'a':
  return 0;
  break;
 
 case 'b':
  return 1;
  break;
 
 case 'c':
  return 2;
  break;
 case 'd':
  return 3;
  break;
 case 'e':
  return 4;
  break;
 case 'f':
  return 5;
  break;
 case 'g':
  return 6;
  break;
 case 'h':
  return 7;
  break;
 default:
  return -1;
  break;
 }
}
void setLEDLow (int number){
 switch (number)
 {
 case 1:
  digitalWrite(LED1,LOW);
  break;
 
 case 2:
  digitalWrite(LED2,LOW);
  break;
 
 case 3:
  digitalWrite(LED3,LOW);
  break;
 case 4:
  digitalWrite(LED4,LOW);
  break;
 case 5:
  digitalWrite(LED5,LOW);
  break;
 case 6:
  digitalWrite(LED6,LOW);
  break;
 case 7:
  digitalWrite(LED7,LOW);
  break;
 case 8:
  digitalWrite(LED8,LOW);
  break;
 default:
  
  break;
 }
}
void set1To8High(){
 digitalWrite(LED1,HIGH);
 digitalWrite(LED2,HIGH);
 digitalWrite(LED3,HIGH);
 digitalWrite(LED4,HIGH);
 digitalWrite(LED5,HIGH);
 digitalWrite(LED6,HIGH);
 digitalWrite(LED7,HIGH);
 digitalWrite(LED8,HIGH);
}
void IRAM_ATTR UpdateLEDMatrix() {
 portENTER_CRITICAL_ISR(&timerMux);
 currentPeriodePart++;
 
 if(currentPeriodePart>=8){
  currentPeriodePart=0;
 }
 set1To8High();
 switch (currentPeriodePart)
 {
 case 0:
  digitalWrite(PINA2, LOW);
  digitalWrite(PINA1, LOW);
  digitalWrite(PINA0, LOW);
  
  break;
 case 1:
  digitalWrite(PINA2, LOW);
  digitalWrite(PINA1, LOW);
  digitalWrite(PINA0, HIGH);
  
  break;
 case 2:
  digitalWrite(PINA2, LOW);
  digitalWrite(PINA1, HIGH);
  digitalWrite(PINA0, LOW);
 
  break;
 case 3:
  digitalWrite(PINA2, LOW);
  digitalWrite(PINA1, HIGH);
  digitalWrite(PINA0, HIGH);
  
  break;
 case 4:
  digitalWrite(PINA2, HIGH);
  digitalWrite(PINA1, LOW);
  digitalWrite(PINA0, LOW);
  
  break;
 case 5:
  digitalWrite(PINA2, HIGH);
  digitalWrite(PINA1, LOW);
  digitalWrite(PINA0, HIGH);
  
  lastLEDSwitch = millis();
  break;
 case 6:
  digitalWrite(PINA2, HIGH);
  digitalWrite(PINA1, HIGH);
  digitalWrite(PINA0, LOW);
  
  break;
 case 7:
  digitalWrite(PINA2, HIGH);
  digitalWrite(PINA1, HIGH);
  digitalWrite(PINA0, HIGH);
  
  lastLEDSwitch = millis();
  break;
 default:
  digitalWrite(PINA2, LOW);
  digitalWrite(PINA1, LOW);
  digitalWrite(PINA0, LOW);
  
  lastLEDSwitch = millis();
  break;
 }
 for (int i = 0; i < 8; i++)
 {
  if(towritetmp[currentPeriodePart][i] == 1){
   setLEDLow(i+1);
  }else if (towritetmp[currentPeriodePart][i] == 2 && ((millis()%1000) <=500)){
   
   setLEDLow(i+1);
  }
  
  
 }
 
 portEXIT_CRITICAL_ISR(&timerMux);
}
void IRAM_ATTR fieldPressed() {  
 Serial.println("A register had an interrupt!");  
  resetMCP=false;  
  triggeredInterrupt=true;  
  timePressed=millis(); 
  stop=true; 
  if(unfinishedPress.equals("")&&(positionDetectionPressDelay+750>millis())){
   deleteNextDetectedPress=true;
  }
  
}  
void positionDetectionSetup() {   
   
 Serial.println("Starting Port expander");   
 while (!mcp.begin_I2C(32U)) {   
  Serial.println("Error.");   
  delay(3000);  
 }
 pinMode(INTAPin,INPUT); 
 mcp.pinMode(0,INPUT);  
 mcp.pinMode(1,INPUT);  
 mcp.pinMode(2,INPUT);  
 mcp.pinMode(3,INPUT);   
 mcp.pinMode(4,INPUT);  
 mcp.pinMode(5,INPUT);  
 mcp.pinMode(6,INPUT);  
 mcp.pinMode(7,INPUT);  
 mcp.pinMode(8,OUTPUT);  
 mcp.pinMode(9,OUTPUT);  
 mcp.pinMode(10,OUTPUT);  
 mcp.pinMode(11,OUTPUT); 
 mcp.pinMode(12,OUTPUT);  
 mcp.pinMode(13,OUTPUT);  
 mcp.pinMode(14,OUTPUT);  
 mcp.pinMode(15,OUTPUT);   
 mcp.writeGPIOB(0b00000000); 
 mcp.setupInterruptPin(0,CHANGE);   
 mcp.setupInterruptPin(1,CHANGE);  
 mcp.setupInterruptPin(2,CHANGE);  
 mcp.setupInterruptPin(3,CHANGE);   
 mcp.setupInterruptPin(4,CHANGE);   
 mcp.setupInterruptPin(5,CHANGE);  
 mcp.setupInterruptPin(6,CHANGE);  
 mcp.setupInterruptPin(7,CHANGE);  
 attachInterrupt(digitalPinToInterrupt(INTAPin), fieldPressed, FALLING);  
 mcp.getLastInterruptPin();  
 
}
void positionDetectionloop(void * pvParameters) {  
while(true){
 vTaskDelay(10);
 

if(lastpressedbutton.equals("")){
 if(millis()%250>=125 && activeButton==2&& (lastInterruptTime+buttonDelay<=millis())){ 
 buttonInterruptAttachement=false;
 activeButton=5;
 digitalWrite(TasterSteuerungsPin2,HIGH);
 digitalWrite(TasterSteuerungsPin5,LOW);
 lastButtonTime=millis();
 
 }else if(millis()%250<125 && activeButton==5 && (lastInterruptTime+buttonDelay<=millis())){
 
 buttonInterruptAttachement=false;
 activeButton=2;
 digitalWrite(TasterSteuerungsPin5,HIGH);
 digitalWrite(TasterSteuerungsPin2,LOW);
 lastButtonTime=millis();
 
 }
}
 if(lastPositionDetection+PositionDetectionDelay<=millis()){
  //Serial.print("INTA:");
  //Serial.println(digitalRead(INTAPin));
 int periodePart =-1; 
 int currentperiodePartPosition= millis(); 
 currentperiodePartPosition%=TPositionDetection; 
 currentperiodePartPosition/=(TPositionDetection/8);
 if(moveToTransmit.equals("")){
  if(unfinishedPress.equals("")){
  if(triggeredInterrupt){ 
   timePressed= timePressed%TPositionDetection; 
   periodePart = ((int)timePressed)/(TPositionDetection/8); 
   Serial.println(periodePart); 
   timePressed=0; 
  } 
  if(periodePart!=-1){ 
   Serial.print("Triggered Position: "); 
   int pin= mcp.getLastInterruptPin()+1; 
   resetMCP=true;
   unfinishedPress= numberToLetter(periodePart);
   unfinishedPress+=pin;
   Serial.println(unfinishedPress); 
   if(deleteNextDetectedPress){ 
    Serial.print("Fehler/Zu schnell:");
    Serial.println(unfinishedPress);
    unfinishedPress="";
    stop=false;
    deleteNextDetectedPress=false;
    positionDetectionPressDelay=millis();
   } 
   triggeredInterrupt=false; 
  } 
  }else {
  if(triggeredInterrupt){ 
   
   int pin= mcp.getLastInterruptPin()+1; 
   resetMCP=true;
   if(pin == unfinishedPress.substring(1,2).toInt()){
   Serial.print("Feld erkannt:");
   Serial.println(unfinishedPress);
   unfinishedMove+=unfinishedPress;
   if(unfinishedMove.length()>=4){
    moveToTransmit=unfinishedMove;
    if(moveToTransmit.substring(0,2).equals(moveToTransmit.substring(2,4))){
      moveToTransmit="";
      Serial.println("Zug zurueckgenommen");
    }
    unfinishedMove="";
   Serial.print("Zug erkannt:");
   Serial.println(moveToTransmit);
   }
   }else{
   Serial.print("Abgebrochene Felderkennung:");
   Serial.println(unfinishedPress);
   }
  
  unfinishedPress = "";
  triggeredInterrupt = false;
  stop=false;
  positionDetectionPressDelay=millis();
  } 
  }
 }
 int tmpPINAvalue= digitalRead(INTAPin);
 if(resetMCP && (tmpPINAvalue==0)){
  mcp.readGPIO(mcp.getLastInterruptPin());
 }
 if(!stop){ 
  switch (currentperiodePartPosition) 
  { 
  case 0: 
  //Serial.println("AAA"); 
  tmplastposition="A";
  mcp.writeGPIOB(0b00000001); 
  break; 
  case 1: 
  //Serial.println("BBB"); 
  tmplastposition="B";
  mcp.writeGPIOB(0b00000010); 
  break; 
  case 2: 
  //Serial.println("CCC"); 
  tmplastposition="C";
  mcp.writeGPIOB(0b00000100); 
  break; 
  case 3:  
  //Serial.println("DDD"); 
  tmplastposition="D";
  mcp.writeGPIOB(0b00001000); 
  break; 
  case 4:  
  mcp.writeGPIOB(0b00010000); 
  tmplastposition="E";
  break; 
  case 5:  
  mcp.writeGPIOB(0b00100000); 
  tmplastposition="F";
  break; 
  case 6: 
  mcp.writeGPIOB(0b01000000); 
  tmplastposition="G";
  break; 
  case 7:  
  mcp.writeGPIOB(0b10000000); 
  tmplastposition="H";
  break; 
  default: 
  mcp.writeGPIOB(0b00000000); 
  tmplastposition="Null";
  break; 
  } 
 }
 lastPositionDetection=millis();
 }
 
}
} 
String numberToLetter(int number){ 
 switch (number) 
 { 
 case 0: 
  return "a"; 
  break; 
 case 1: 
  return "b"; 
  break; 
 case 2: 
  return "c"; 
  break; 
 case 3: 
  return "d"; 
  break; 
 case 4: 
  return "e"; 
  break; 
 case 5: 
  return "f"; 
  break; 
 case 6: 
  return "g"; 
  break; 
 case 7: 
  return "h"; 
  break; 
 default: 
  return ""; 
  break; 
 } 
} 
void resetAllLEDs(){
 for(int i =0;i<8;i++){
  
  for(int k=0;k<LEDList[i].size(); k=k){
   
   int tmpNumber=LEDList[i].get(k);
   if(tmpNumber>=100){
    tmpNumber-=100;
   }
   String tmpPosition=numberToLetter(i)+tmpNumber;
   markPosition(tmpPosition,false,false);
   
  }
 
 }
}
void refreshLEDMatrix(){
 resetAllLEDs();
 String nextMove="";
 String pawnPromotion="";
 if(illegalMoves.size()==0 && !enemyMove.equals("")){
  nextMove=enemyMove;
  if(nextMove.length()==5){
   pawnPromotion=nextMove.substring(4,5);
   nextMove=nextMove.substring(0,4);
  }
  
 }else{
  String tmpmove = illegalMoves.get(illegalMoves.size()-1);
  nextMove = tmpmove.substring(2,4)+ tmpmove.substring(0,2); // reverse Move
  //Bauernumwandlung wird schon automatisch berücksichtigt!
 }
 if(!nextMove.equals("")){
  
  if(nextMove.substring(0,4).equals(moveToTransmit.substring(0,4)) && !pawnPromotion.equals("")){
   markPosition(nextMove.substring(0,2),true,false);
   markPosition(nextMove.substring(2,4),true,false);
   if(pawnPromotion.equals("q")){
    markPosition("d1",true,true);
    markPosition("d8",true,true);
   }else if(pawnPromotion.equals("n")){
    markPosition("b1",true,true);
    markPosition("b8",true,true);
    markPosition("g1",true,true);
    markPosition("g8",true,true);
   }else if(pawnPromotion.equals("r")){
    markPosition("a1",true,true);
    markPosition("a8",true,true);
    markPosition("h1",true,true);
    markPosition("h8",true,true);
   }else if(pawnPromotion.equals("b")){
    markPosition("c1",true,true);
    markPosition("c8",true,true);
    markPosition("f1",true,true);
    markPosition("f8",true,true);
   }
  }else if(unfinishedMove.length()==0){
   
   markPosition(nextMove.substring(0,2),true,true);
  } else if(unfinishedMove.length()>=2){
   
   if(nextMove.substring(0,2).equals(unfinishedMove.substring(0,2))){
    markPosition(nextMove.substring(0,2),true,false);
    markPosition(nextMove.substring(2,4),true,true);
   }else {
    markPosition(nextMove.substring(0,2),true,true);
    markPosition(unfinishedMove.substring(0,2),true,false);
   }
  }
 }else if(unfinishedMove.length()>=2){
  markPosition(unfinishedMove.substring(0,2),true,false);
 }else if(moveToTransmit.length()>=4){
  if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa")){
   if(moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8){
    markPosition(moveToTransmit.substring(0,2),true,true);
    markPosition(moveToTransmit.substring(2,4),true,true);
   }
  }else{
   markPosition(moveToTransmit.substring(0,2),true,false);
   markPosition(moveToTransmit.substring(2,4),true,false);
  }
  
  
 }
 for(int i =0; i<8;i++){
  for(int j =0; j<8;j++){
   towritetmp[i][j] =0;
  }  
 }
 for(int i =0; i<8;i++){
  for (int j = 0; j < LEDList[i].size(); j++)
  {
   int ledIndex =LEDList[i].get(j);
   if(ledIndex>=100){ //100 ist der Offset furs Blinken
    //if((millis()%1000) <= 500){
     towritetmp[i][ledIndex-101] =2;
     //setLEDLow(ledIndex-100); //100 ist der Offset furs Blinken
    //}
   }else{
    towritetmp[i][ledIndex-1] =1;
    //setLEDLow(ledIndex); //100 ist der Offset furs Blinken
   }
  }
 }
 
}
void IRAM_ATTR laueferInterrupt() {  
 
 if(millis()-lastInterruptTime >=interruptDelay){
  if(millis()-lastInterruptTime >=interruptDelay){
   if(activeButton == 2){
    if(lastpressedbutton.equals("Laeufer")){
     Serial.println("Laeufer");
     if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa") && (moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8) && moveToTransmit.length()<5){
      moveToTransmit+="b";
     }
     lastpressedbutton="";
    }else{
     lastpressedbutton="Laeufer";
    }
    
   }else if(activeButton == 5){
    if(lastpressedbutton.equals("Turm")){
     Serial.println("Turm");
     if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa") && (moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8) && moveToTransmit.length()<5){
      moveToTransmit+="r";
     }
     lastpressedbutton="";
    }else{
     lastpressedbutton="Turm";
    }
   }
  } 
 }
 lastInterruptTime=millis();
}  
void IRAM_ATTR springerInterrupt() {   
 if(millis()-lastInterruptTime >=interruptDelay){
  if(activeButton == 2){
   if(lastpressedbutton.equals("Bauer")){
    Serial.println("Bauer");
    if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa") && (moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8) && moveToTransmit.length()<5){
     moveToTransmit+="q";
    }
    lastpressedbutton="";
   }else{
    lastpressedbutton="Bauer";
   }
   
  }else if(activeButton == 5){
   if(lastpressedbutton.equals("Springer")){
    Serial.println("Springer");
    if(chessBoard[moveToTransmit.substring(1,2).toInt()][letterToNumber(moveToTransmit.substring(0,1))].substring(1,3).equals("pa") && (moveToTransmit.substring(3,4).toInt()==1 || moveToTransmit.substring(3,4).toInt()==8) && moveToTransmit.length()<5){
     moveToTransmit+="n";
    }
    lastpressedbutton="";
   }else{
    lastpressedbutton="Springer";
   }
  }
 } 
 lastInterruptTime=millis();
} 
void IRAM_ATTR dameInterrupt() {  
 
 if(millis()-lastInterruptTime >=interruptDelay){
  if(activeButton == 2){
   if(lastpressedbutton.equals("Dame")){
    Serial.println("Dame");
    lastpressedbutton="";
   }else{
    lastpressedbutton="Dame";
   }
   
  }else if(activeButton == 5){
   if(lastpressedbutton.equals("Koenig")){
    Serial.println("Koenig");
    lastpressedbutton="";
   }else{
    lastpressedbutton="Koenig";
   }
  }
 } 
 lastInterruptTime=millis();
} 
void printChessBoard(){
 for (size_t i = 8; i > 0; i--)
 {
  for (size_t k = 0; k < 8; k++)
  {
   if(!chessBoard[i][k].equals("")){
    Serial.print(chessBoard[i][k]);
    Serial.print("  ");
   }else{
    Serial.print(" , ");
    Serial.print("  ");
    
   }
   
  }
  Serial.println();
 }
}
void initChessBoard(){
 // first row
 chessBoard[1][0] ="wroc";
 chessBoard[1][1] ="wkn0";
 chessBoard[1][2] ="wbi0";
 chessBoard[1][3] ="wqu0";
 chessBoard[1][4] ="wkic";
 chessBoard[1][5] ="wbi0";
 chessBoard[1][6] ="wkn0";
 chessBoard[1][7] ="wroc";
 // second row
 chessBoard[2][0] ="wpas";
 chessBoard[2][1] ="wpas";
 chessBoard[2][2] ="wpas";
 chessBoard[2][3] ="wpas";
 chessBoard[2][4] ="wpas";
 chessBoard[2][5] ="wpas";
 chessBoard[2][6] ="wpas";
 chessBoard[2][7] ="wpas";
 // seventh row
 chessBoard[7][0] ="bpas";
 chessBoard[7][1] ="bpas";
 chessBoard[7][2] ="bpas";
 chessBoard[7][3] ="bpas";
 chessBoard[7][4] ="bpas";
 chessBoard[7][5] ="bpas";
 chessBoard[7][6] ="bpas";
 chessBoard[7][7] ="bpas";
 // tenth row
 chessBoard[8][0] ="broc";
 chessBoard[8][1] ="bkn0";
 chessBoard[8][2] ="bbi0";
 chessBoard[8][3] ="bqu0";
 chessBoard[8][4] ="bkic";
 chessBoard[8][5] ="bbi0";
 chessBoard[8][6] ="bkn0";
 chessBoard[8][7] ="broc";
 printChessBoard();
}
void updateChessBoard(String moveToUpdate){
 moveToUpdate.toLowerCase();
 String tmppiece ="";
 tmppiece=chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(0,1))];
 String special= tmppiece.substring(3,4);
 if(moveToUpdate.length()==4){
  
  //Rochade Turm
  if(moveToUpdate.equals("e1g1")){
   if(chessBoard[1][letterToNumber("H")].equals("wroc") && chessBoard[1][letterToNumber("E")].equals("wkic")){
    tmppiece=chessBoard[1][letterToNumber("H")].substring(0,3)+"0";
    chessBoard[1][letterToNumber("H")] ="";
    chessBoard[1][letterToNumber("F")] = tmppiece;
   }
  }else if(moveToUpdate.equals("e1c1")){
   if(chessBoard[1][letterToNumber("A")].equals("wroc") && chessBoard[1][letterToNumber("E")].equals("wkic")){
    tmppiece=chessBoard[1][letterToNumber("A")].substring(0,3)+"0";
    chessBoard[1][letterToNumber("A")] ="";
    chessBoard[1][letterToNumber("D")] = tmppiece;
   }
  }else if(moveToUpdate.equals("e8g8")){
   if(chessBoard[8][letterToNumber("H")].equals("broc") && chessBoard[8][letterToNumber("E")].equals("bkic")){
    tmppiece=chessBoard[8][letterToNumber("H")].substring(0,3)+"0";
    chessBoard[8][letterToNumber("H")] ="";
    chessBoard[8][letterToNumber("F")] = tmppiece;
   }
  }else if(moveToUpdate.equals("e8c8")){
   if(chessBoard[8][letterToNumber("A")].equals("broc") && chessBoard[8][letterToNumber("E")].equals("bkic")){
    tmppiece=chessBoard[8][letterToNumber("A")].substring(0,3)+"0";
    chessBoard[8][letterToNumber("A")] ="";
    chessBoard[8][letterToNumber("D")] = tmppiece;
   }
  }
  tmppiece=chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(0,1))]; //resets if casteling happend
  if(special.equals("c")){
   //Rochade Spezialfall
   tmppiece=tmppiece.substring(0,3)+"0";
  }else if(special.equals("s")){
   // en passant: Markieren der en passant fähigen Bauer;
   // s = Bauer wurde noch nicht bewegt
   // e = Bauer ging zwei Felder vor
   if(moveToUpdate.substring(1,2).toInt() == 2 && moveToUpdate.substring(3,4).toInt() == 4){
    tmppiece=tmppiece.substring(0,3)+"e";
   }else if(moveToUpdate.substring(1,2).toInt() == 7 && moveToUpdate.substring(3,4).toInt() == 5){
    tmppiece=tmppiece.substring(0,3)+"e";
   }else{
    tmppiece=tmppiece.substring(0,3)+"0";
   }
  
  }else if(special.equals("e")){
   tmppiece=tmppiece.substring(0,3)+"0";
  } 
  // en passant: berücksichtigen der gegnerischen Figur
  if(letterToNumber(moveToUpdate.substring(0,1)) != letterToNumber(moveToUpdate.substring(2,3)) && tmppiece.substring(1,3).equals("pa")){
   String playerside = tmppiece.substring(0,1);
   String enemyside="";
   if(playerside.equals("w")){
    enemyside="b";
   }else if(playerside.equals("b")){
    enemyside="w";
   }
   if(chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(2,3))].equals(enemyside+"pae")){
    chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(2,3))] = "";
   }
  }
  // Nachmachen des Zuges
  chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(0,1))] ="";
  chessBoard[moveToUpdate.substring(3,4).toInt()][letterToNumber(moveToUpdate.substring(2,3))] = tmppiece;
  
  
 }else if(moveToUpdate.length()==5){
  // Bauerumwandlung
  String tmppawntopiece ="";
  String pieceshort=moveToUpdate.substring(4,5);
  if(pieceshort.equals("q")){
   tmppawntopiece="qu";
  }else if(pieceshort.equals("b")){
   tmppawntopiece="bi";
  }else if(pieceshort.equals("r")){
   tmppawntopiece="ro";
  }else if(pieceshort.equals("n")){
   tmppawntopiece="kn";
  }
  chessBoard[moveToUpdate.substring(1,2).toInt()][letterToNumber(moveToUpdate.substring(0,1))] ="";
  chessBoard[moveToUpdate.substring(3,4).toInt()][letterToNumber(moveToUpdate.substring(2,3))] = tmppiece.substring(0,1)+tmppawntopiece+"0";
 }
 
 
}
void castlingHandling(String moveToCheck){
 if(moveToCheck.equals("e1g1")){
   if(chessBoard[1][letterToNumber("H")].equals("wroc") && chessBoard[1][letterToNumber("E")].equals("wkic")){
    // Zug: h1f1
    illegalMoves.add("f1h1"); //gespiegelter Zug, da es als "illegaler" Zug gespeichert wird
   }
  }else if(moveToCheck.equals("e1c1")){
   if(chessBoard[1][letterToNumber("A")].equals("wroc") && chessBoard[1][letterToNumber("E")].equals("wkic")){
    // Zug: a1d1
    illegalMoves.add("d1a1"); //gespiegelter Zug, da es als "illegaler" Zug gespeichert wird
   }
  }else if(moveToCheck.equals("e8g8")){
   if(chessBoard[8][letterToNumber("H")].equals("broc") && chessBoard[8][letterToNumber("E")].equals("bkic")){
    // Zug: h8f8
    illegalMoves.add("f8h8"); //gespiegelter Zug, da es als "illegaler" Zug gespeichert wird
   }
  }else if(moveToCheck.equals("e8c8")){
   if(chessBoard[8][letterToNumber("A")].equals("broc") && chessBoard[8][letterToNumber("E")].equals("bkic")){
    // Zug: a8d8
    illegalMoves.add("d8a8"); //gespiegelter Zug, da es als "illegaler" Zug gespeichert wird
   }
  }
}
