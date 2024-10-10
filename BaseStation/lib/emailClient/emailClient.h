#ifndef EMAIL_CLIENT_H
#define EMAIL_CLIENT_H

#include <ESP_Mail_Client.h>

//Struct to hold data for email alerts
struct AlertData
{
  uint8_t nodeID;
  uint8_t isWarning;
};

#define manifestFileName "/manifest.json"

//Emails to held in the queue
#define EMAIL_QUEUE_LENGTH 10

//SMTP server details
#define SMTP_HOST "smtp.gmail.com"

#define SMTP_PORT esp_mail_smtp_port_465

/* The log in credentials */
#define AUTHOR_EMAIL "projectfishworks@gmail.com"
#define AUTHOR_PASSWORD "qfow uumv dyzg iexf"


/* Recipient email address */
#define RECIPIENT_EMAIL "sebastien@robitaille.info"

void sendEmailToQueue(AlertData *data);
//Sends email based on the contents of the AlertData
void sendEmail(AlertData *data);

void initEmailClient();

void emailClientLoop();

// Callback function to get the Email sending status
void smtpCallback(SMTP_Status status);

#endif