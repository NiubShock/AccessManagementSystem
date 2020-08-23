#include <SPI.h>
#include <MFRC522.h>
#include <Bridge.h>
#include <FileIO.h>
#include <YunServer.h>
#include <YunClient.h>

//###################################################################RFID
#define SS_PIN 10                                                   //pin for Slave Select
#define RST_PIN 9                                                   //Reset pin
MFRC522 rfid(SS_PIN, RST_PIN);                                      // Instance of the class
MFRC522::MIFARE_Key key; 
byte nuidPICC[4];                                                   // Init array that will store new NUID 


//###################################################################SERVER MANAGEMENT + CLIENT
YunServer server;                                                   //our server

String newEntryList;                                                //contains the string that must be added to the LIST file
String newEntryData;                                                //contains the string that must be added to the DATA file

const char pathList[] = "/mnt/sda1/arduino/www/Project/www/List";   //path of the LIST file -> contains people that have entered
const char pathData[] = "/mnt/sda1/arduino/www/Project/www/Data";   //path of DATA file -> contains the personal info for UID


const char pathAccess[] = "/mnt/sda1/arduino/www/Project/www/accesstype";   //path of accesstype file
const char accessAllowed[] = "Welcome back!";                               //string for the access file
const char accessForb[] = "User not registered!";                           //string for the access file
const char newUser[] = "New user registered!";                              //string for the access file

//setup all the different parts required
void setup() { 
  //Serial for dubug infomation
  Serial.begin(9600);

  //Setup everything for the RFID
  //Init SPI bus
  SPI.begin(); 
  //Init MFRC522 
  rfid.PCD_Init();

  //set the initial dummy value for the RFID
  for (byte i = 0; i < 6; i++) {
    key.keyByte[i] = 0xFF;
  } 

  //start the bridge to allow the communication
  Bridge.begin();

  //start the server to listen to local's call only
  server.listenOnLocalhost();
  server.begin();

  Serial.println("Web browser started");

  writeAccess(accessAllowed, sizeof(accessAllowed));
}


void loop() {

  //wait for any client to write -> this happens only when a new user is registered
  YunClient client = server.accept();
  if(client){
    //call the process function -> obtain the data and check the different parts of it
    process(client);
    client.stop();
  }

  //Wait for new card
  if (!rfid.PICC_IsNewCardPresent()){
    return;
  }else{
    //new card present -> check if already in database or must be registered fisrt
    readCard();
  }  
}


//Separate the different part received with the REST api
void process(YunClient client){

  //get the command first
  String command = client.readStringUntil('/');
  
  
  //case of new registration required
  if(command == "registration"){
    
    //separate the single information
    String _name = client.readStringUntil('/');
    String _surname = client.readStringUntil('/');  
    String _time = client.readStringUntil('/');
    String _email = client.readStringUntil('/');
    String _CAnumber = client.readStringUntil('\r');
    //create the overall string -> the string in the LIST file don't need the RFID
    newEntryList = _name + "|" + _surname + "|" + _time + "|" + _email + "|" + _CAnumber;
  
    //create the string with RFID UID
    char temp[4];
    for(int i = 0; i < 4; ++i){
      temp[i] = nuidPICC[i];
    }
    String _UID = String(temp);
    newEntryData = _UID + "|" + _name + "|" + _surname + "|" + _email + "|" + _CAnumber;
    
    //add the data to the list and the database
    addEntryToData(newEntryData);
    addEntryToAccess(newEntryList);
    writeAccess(newUser, sizeof(newUser));
    
  }else{
    //TODO:call the python script here!
    Process p;

    String comm = "/mnt/sda1/arduino/www/Project/www/mailSend.py";
    comm += " ";
    comm += client.readStringUntil('\r');
    Serial.println(comm);
    p.runShellCommand(comm);
    //p.read();
    
  }
}


void readCard(){

  // Verify if the NUID has been readed -> avoid double reading
  if (!rfid.PICC_ReadCardSerial()){
    return;
  }else{
    Serial.println("Card detected");

    // Store NUID into nuidPICC array
    for (byte i = 0; i < 4; i++) {
      nuidPICC[i] = rfid.uid.uidByte[i];
    }

    //show the new RFID
    Serial.print(F("The NUID tag is:"));
    printHex(rfid.uid.uidByte, rfid.uid.size);
  }

  //check if the card is already registered
  if(isRegistered()){
    Serial.println("The card is already registered");
    writeAccess(accessAllowed, sizeof(accessAllowed));
    //if is registered just add to the list

    //get the data starting from the UID
    String mData = getDataFromUID();

    //change the string and add the time
    String mString;   //final String
    int i;            //counter for "for" cycle -> outside cause must be reused
    int occ = 0;      //number of | -> the date must be after the second
    for(i = 0; i < mData.length(); ++i){
      //increase the number of occurrence
      if(mData.charAt(i) == '|'){
        occ++;
      }
      //if we reach the 2 occurrence than stop, otherwise keep adding the character
      if(occ < 2){
        mString = mString + mData.charAt(i);
      }else{
        break;
      }
    }
    //add the date
    mString = mString + '|' + getDate();
    //add the remaning part of the string -> i reuse
    for(; i < mData.length(); ++i){
      mString = mString + mData.charAt(i);
    }
    
    //write them in the List file    
    addEntryToAccess(mString);
  }else{
    writeAccess(accessForb, sizeof(accessForb));
    Serial.println("The card is not registered");
    //TODO: add the write of the file user_access
    //the data are received in the process() function
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
  
}


//this function add the string to the list file
bool addEntryToAccess(String mClientData){
  //verify the existance of the file
  if(FileSystem.exists(pathList)){
    Serial.println("File read successfully");
    
    //open the file for write at the end of it
    File file = FileSystem.open(pathList, FILE_APPEND);
    
    //create a dynamic pointer with the size equal to the data to write
    char *temp_ptr;
    uint8_t dimension = mClientData.length();
    temp_ptr = (char*)malloc(sizeof(char) * dimension);
    //TODO:check if the conversion works correctly
    mClientData.toCharArray(temp_ptr, dimension);

     
    //write the data to the file
    file.write(temp_ptr, dimension);
    //add the new line
    file.write('\r');
    file.write('\n');   
    
    //close and free all the variables
    free(temp_ptr);
    file.close();
    
  }else{
    return false;
  }
  return true;
}


//this function check if the card is in the database
bool isRegistered(){
  //create the string with RFID UID
  char temp[4];
  for(int i = 0; i < 4; ++i){
    temp[i] = nuidPICC[i];
  }
  
  if(searchString(temp) > 0){
    return true;
  }else{
    return false;
  }
}


//add the card to the database with the process()'s info
bool addEntryToData(String str){
  
  if(FileSystem.exists(pathData)){
    //open the file for write at the end of it
    File file = FileSystem.open(pathData, FILE_APPEND);

    //create a char[] with the size required
    char *temp_ptr;
    uint8_t dimension = str.length();
    temp_ptr = (char*)malloc(sizeof(char) * dimension);
    //load here the data
    str.toCharArray(temp_ptr, dimension);

    //write the data first
    file.write(temp_ptr, dimension);
    file.write('\r');
    file.write('\n');

    //close and free all the variables
    free(temp_ptr);
    file.close();
    
  }else{
    //file not there
    return false;
  }

  return true;
}


//obtain the data from the UID -> check into the database
String getDataFromUID(){
  String output;    //contains the output
  char temp_read;   //temporaneal read of the char of the file
  int position;     //position of the string

  //create the string with RFID UID
  char temp[4];
  for(int i = 0; i < 4; ++i){
    temp[i] = nuidPICC[i];
  }
  //get the position of the string to fetch
  position = searchString(temp);
  if(position > 0){
    //open the file
    File file = FileSystem.open(pathData, FILE_READ);
    //move into the file, the +1 is to avoid the | character
    file.seek(position + 1);
    //read the next character   
    temp_read = file.read();
    //as long as the line is not finished
    while(temp_read != '\n'){
      //add the char to our string
      output += temp_read;
      //read the next character
      temp_read = file.read();
    }
    file.close();
  }else{
    //notify issue
  }
  return(output);
}


//return a positive number (0 included) if find the string -> represent the position
//return -1 if string not found
int searchString(char mChar[]){
  int seqCout = 0;    //variable for sequential true cond -> need 4 for the UID
  int position = -1;
  char temp;          //temporal storage of the character from the file

  File file = FileSystem.open(pathData, FILE_READ);

  for(int i = 0; i < file.size(); ++i){
    //get a character from the file
    temp = file.read();
    //check if correspond to the UID
    if(temp == mChar[seqCout]){
      //if correspond increase the sequential correction
      seqCout++;
      //at 4 the UID is found
      if(seqCout == 4){
        break;
      }
    }else{
      //if the character doesn't correspond we need to go back if we have for some reason found some char equal
      //reset automatically even the index of the UID
      if(seqCout > 0){
        //TODO: check
        file.seek(file.position() - seqCout + 1);
        seqCout = 0;
      }
    }
  }

  //save the correct position
  if(seqCout == 4){    
    position = file.position();
  }else{
    position = -1;
  }

  //close the file and return the position
  file.close();
  return(position);
  
}

bool writeAccess(char mChar[], int dimension){  
  File file = FileSystem.open(pathAccess, FILE_WRITE);
  file.write(mChar, dimension);

  file.close();

  return true;
}

//function that start a process that print the date in format hh:mm:ss
String getDate(){
  Process p;
  
  p.begin("date");
  p.addParameter("+%T");
  p.run();
  while(p.running());
  return(p.readStringUntil('\n'));
}


void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }

  Serial.println();
}
