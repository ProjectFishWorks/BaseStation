#include "emailClient.h"
#include "ArduinoJson.h"

/* Declare the global used SMTPSession object for SMTP transport */
SMTPSession smtp;

// Declare the global used Session_Config for user defined session credentials
Session_Config config;

QueueHandle_t emailQueue;

void sendEmailToQueue(AlertData *data){
    xQueueSend(emailQueue, data, portMAX_DELAY);
}

void emailClientLoop(char *recipient, char *recipient_name){
    AlertData alertData;
    if(xQueueReceive(emailQueue, &alertData, 1) == pdTRUE) {
      sendEmail(&alertData, recipient, recipient_name);
    }
}

void initEmailClient(){
    //Set the email client
    smtp.debug(1);
    // Set the callback function to get the sending results
    smtp.callback(smtpCallback);

    // Set the session config
    config.server.host_name = SMTP_HOST;
    config.server.port = SMTP_PORT;
    config.login.email = AUTHOR_EMAIL;
    config.login.password = AUTHOR_PASSWORD;

    // Set the secure mode
    config.secure.mode = esp_mail_secure_mode_ssl_tls;

    // Set the NTP config time
    config.time.ntp_server = "pool.ntp.org";
    config.time.gmt_offset = 0;
    config.time.day_light_offset = 3600;

    smtp.keepAlive(5 /* tcp KeepAlive idle 5 seconds */, 5 /* tcp KeeAalive interval 5 seconds */, 1 /* tcp KeepAlive count 1 */);

    emailQueue = xQueueCreate(EMAIL_QUEUE_LENGTH, sizeof(AlertData));
}

void sendEmail(AlertData *data, char *recipient, char *recipient_name){

      /* Declare the message class */
      SMTP_Message message;

      /* Set the message headers */
      message.sender.name = F("Project FishWorks Base Station");
      message.sender.email = AUTHOR_EMAIL;

      // In case of sending non-ASCII characters in message envelope,
      // that non-ASCII words should be encoded with proper charsets and encodings
      // in form of `encoded-words` per RFC2047
      // https://datatracker.ietf.org/doc/html/rfc2047

      //Get the current time
      struct tm timeinfo;
      getLocalTime(&timeinfo);
      char locTime[9];
      sprintf(locTime, "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

      String deviceName = "Node " + String(data->nodeID);

      //Send manifest data to the MQTT broker
      if(LittleFS.exists(manifestFileName)) {
        Serial.println("Manifest file exists");
        File manifestFile = LittleFS.open(manifestFileName, FILE_READ);
        String manifestString = manifestFile.readString();
        manifestFile.close();

        JsonDocument manifestDoc;

        // Parse the JSON object
        DeserializationError error = deserializeJson(manifestDoc, manifestString);

        if(error){
          Serial.println("Error parsing manifest file");
        }
        else{
            //Get the devices array from the manifest file
            JsonArray devices = manifestDoc["Devices"].as<JsonArray>();
            uint16_t index = 0;
            while(!devices[index].isNull()){
              Serial.println("Checking device: " + devices[index]["NodeID"].as<String>());
              JsonObject device = devices[index];
              if(device["NodeID"] == data->nodeID){
                Serial.println("Device found in manifest file");
                deviceName = device["DeviceName"].as<String>();
                break;
              }
              index++;
            }
        }
      }else{
        Serial.println("Manifest file does not exist");
      }

      //Set the subject and content of the email
      if(data->isWarning){
        message.subject = "WARNING Fish Sense " + deviceName;
        message.text.content = "Warning message received from " + deviceName + " at " + String(locTime) +" UTC\n Please check the web app for more details.";
      } else {
        message.subject = "---ALERT--- Fish Sense " + deviceName;
        message.text.content = "Alert message received from " + deviceName + " at " + String(locTime) +" UTC\n Please check the web app for more details.";
      }

      message.addRecipient(recipient_name, recipient);

      message.text.transfer_encoding = "base64"; // recommend for non-ASCII words in message.
      message.text.charSet = F("utf-8"); // recommend for non-ASCII words in message.
      message.priority = esp_mail_smtp_priority::esp_mail_smtp_priority_low;

      if (!smtp.connect(&config))
      {
        MailClient.printf("Connection error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
        return;
      }

      if (!smtp.isLoggedIn())
      {
        Serial.println("Not yet logged in.");
      }
      else
      {
        if (smtp.isAuthenticated())
          Serial.println("Successfully logged in.");
        else
          Serial.println("Connected with no Auth.");
      }

      // Start sending Email
      if (!MailClient.sendMail(&smtp, &message,true))
        MailClient.printf("Error, Status Code: %d, Error Code: %d, Reason: %s\n", smtp.statusCode(), smtp.errorCode(), smtp.errorReason().c_str());
}

/* Callback function to get the Email sending status */
void smtpCallback(SMTP_Status status)
{
  /* Print the current status */
  Serial.println(status.info());

  /* Print the sending result */
  if (status.success())
  {
    // MailClient.printf used in the examples is for format printing via debug Serial port
    // that works for all supported Arduino platform SDKs e.g. SAMD, ESP32 and ESP8266.
    // In ESP8266 and ESP32, you can use Serial.printf directly.

    Serial.println("----------------");
    MailClient.printf("Message sent success: %d\n", status.completedCount());
    MailClient.printf("Message sent failed: %d\n", status.failedCount());
    Serial.println("----------------\n");

    for (size_t i = 0; i < smtp.sendingResult.size(); i++)
    {
      /* Get the result item */
      SMTP_Result result = smtp.sendingResult.getItem(i);

      // In case, ESP32, ESP8266 and SAMD device, the timestamp get from result.timestamp should be valid if
      // your device time was synched with NTP server.
      // Other devices may show invalid timestamp as the device time was not set i.e. it will show Jan 1, 1970.
      // You can call smtp.setSystemTime(xxx) to set device time manually. Where xxx is timestamp (seconds since Jan 1, 1970)

      MailClient.printf("Message No: %d\n", i + 1);
      MailClient.printf("Status: %s\n", result.completed ? "success" : "failed");
      MailClient.printf("Date/Time: %s\n", MailClient.Time.getDateTimeString(result.timestamp, "%B %d, %Y %H:%M:%S").c_str());
      MailClient.printf("Recipient: %s\n", result.recipients.c_str());
      MailClient.printf("Subject: %s\n", result.subject.c_str());
    }
    Serial.println("----------------\n");

    // You need to clear sending result as the memory usage will grow up.
    smtp.sendingResult.clear();
  }
}