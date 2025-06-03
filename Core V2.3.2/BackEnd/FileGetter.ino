/*

File Getter

The File Getter will HTTP GET a file from the server, and act accordingly on it based on the "Type" given over the websocket.

This enables the Shlug to get updated firmware, TLS certs, lists of users for offline approval, etc.

This also enables a framework for providing things like audio for future shlugs to play. 

*/

void FileGetter(void *pvParameters){
  while(1){
    if(!GetFile){
      //There isn't a file to get yet
      delay(100);
      continue;
    }
    //We got orders to go get a file, connect to the server at the provided path;
    if(CACert == ""){
      //If there is no CACert loaded currently, don't use one
      //Used during dev, probably want to remove since we will hopefully never not have a cert
      client.setInsecure();
    } else{
      client.setCACert(CACert.c_str());
    }
    //In case resources moved, follow redirects.
    http.useHTTP10(true);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    String GetServer = Server + "/" + FilePath;
    if(!http.begin(client, GetServer.c_str())){
      //Client failed to connect to server
      if(DebugMode){
        Serial.println(F("FileGetter failed to begin client."));
      }
      abortGet("Client Begin Fail");
      continue;
    }
    int httpCode = http.GET();
    if(httpCode != 200){
      if(DebugMode){
        Serial.print(F("FileGetter got an unexpected HTTP code: "));
        Serial.println(httpCode);
      }
      abortGet("Bad HTTP Code " + String(httpCode));
      continue;
    }
    //Before we continue, figure out what kind of file we are dealing with.
    if(FileType == "OTA"){
      //This is an OTA update to install
      //1. Open a stream to the file
      //2. Install it using the ESP updater library
      //3. Set the return based on the error, or if we are ok.
      //4. Server will restart us when ready and we will boot into the new app.
      if(DebugMode){
        Serial.println(F("FileGetter Starting OTA!"));
      }
      String AbortOTA;
      NetworkClient *client = http.getStreamPtr();
      int OTASize = http.getSize();
      if (!Update.begin(OTASize)){
        if(DebugMode){
          Serial.print(F("Updater failed to start, error:"));
          Serial.println(String(Update.errorString()));
        }
        AbortOTA = String("Failed to start OTA: ") + String(Update.errorString());
        abortGet(AbortOTA);
        continue;
      }
      if(Update.writeStream(*client) != OTASize){
        if(DebugMode){
          Serial.print(F("Failed to write OTA stream with error:"));
          Serial.println(String(Update.errorString()));
        }
        AbortOTA = String("Failed to write OTA stream: ") + String(Update.errorString());
        abortGet(AbortOTA);
        continue;
      }
      if(!Update.end()){
        if(DebugMode){
          Serial.print(F("Failed to end OTA update with error: "));
          Serial.println(String(Update.errorString()));
        }
        AbortOTA = String("Failed to end OTA: ") + String(Update.errorString());
        abortGet(AbortOTA);
        continue;
      }
      if(DebugMode){
        Serial.println(F("OTA update installed properly."));
      }
      abortGet("OTA Done.");
    } else
    if((FileType == "Staff") || (FileType == "Makers") || FileType == "Cert"){
      //All 3 of these file types have the same general approach;
        //* Save them to SPIFFS named New(whatever).txt
        //* Run a test on them to make sure we have the full, proper file
        //* Rename it to (whatever).txt if it is correct
      //So everything is combined into one here;
      //1. Make a new file of the right name
      String FileName = "/New" + FileType + ".txt";
      File file = SPIFFS.open(FileName, FILE_WRITE);
      //TODO actually save the file, and verify it somehow
    } else{
      //This is an unrecognized or unsupported type
      //Respond to the server with "BadType"
      if(DebugMode){
        Serial.print("Asked to get unexepected/unsupported file type: ");
        Serial.println(FileType);
      }
      abortGet("BadType");
      continue;
    }
  }
}

void abortGet(String Reason){
  Got = Reason;
  DoneGetting = 1;
  while(DoneGetting){
    //Delay until the websocket reports the abort.
    delay(10);
  }
}