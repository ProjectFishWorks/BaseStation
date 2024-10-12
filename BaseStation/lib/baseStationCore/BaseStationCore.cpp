#include <BaseStationCore.h>

BaseStationCore::BaseStationCore(){
}


twai_message_t BaseStationCore::create_message(uint32_t id, uint64_t *data) {
  twai_message_t message;

  message.extd = 1;
  message.identifier = id;
  message.data_length_code = 8;
  memcpy(message.data, data, 8);
  return message;
}

bool BaseStationCore::Init(std::function<void(uint8_t nodeID, uint16_t messageID, uint64_t data)> onMessageReceived, uint8_t nodeID)
{
  this->debug = debug;
  this->onMessageReceived = onMessageReceived;
  this->nodeID = nodeID;

  // Initialize configuration structures using macro initializers
  twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, 
                                                              (gpio_num_t)RX_PIN,
                                                              TWAI_MODE_NORMAL);
  //TODO: deside on the timing config                                                      
  twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  // Install TWAI driver
  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
    Serial.println("Driver installed");
  } else {
    Serial.println("Failed to install driver");
    return false;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    Serial.println("Driver started");
  } else {
    Serial.println("Failed to start driver");
    return false;
  }

  // Reconfigure alerts to detect frame receive, Bus-Off error and RX queue full states
  //Not really sure what most of this means
  uint32_t alerts_to_enable = TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | TWAI_ALERT_BUS_ERROR | TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_BUS_OFF;
  if (twai_reconfigure_alerts(alerts_to_enable, NULL) == ESP_OK) {
    Serial.println("CAN Alerts reconfigured");
  } else {
    Serial.println("Failed to reconfigure alerts");
    return false;
  }

  //Create tx_queue and rx_queue
  tx_queue = xQueueCreate(TX_QUEUE_LENGTH, sizeof(twai_message_t));
  rx_queue = xQueueCreate(RX_QUEUE_LENGTH, sizeof(twai_message_t));

  //Start tasks
  xTaskCreate(this->start_receive_to_rx_queue_task, 
              "start_rx_task_impl", 
              10000, //TODO: Not sure what the stack size should be for this task, 10000 is just a guess, 2048 is default but causes stack overflow crashes in base station
              this, 
              10, 
              NULL);

  xTaskCreate(this->start_rx_queue_event_task, 
              "start_rx_queue_event_task_impl", 
              10000, 
              this, 
              20, 
              NULL);

  //Create task to transmit messages from tx_queue
  xTaskCreate(&BaseStationCore::transmit_tx_queue, 
              "transmit_tx_queue", 
              10000, 
              &tx_queue, 
              30, 
              NULL);

  //Return true since everything is successful
  return true;

}

void BaseStationCore::transmit_tx_queue(void *queue) {

  while(1){
    twai_message_t message;

    if(xQueueReceive(*(QueueHandle_t *) queue, &message, RX_TX_BLOCK_TIME) == pdTRUE){
      Serial.println("Message transmitted with data: " + String(*message.data));
      //If there is a message in the queue, try to transmit it
      if (twai_transmit(&message, 1) == ESP_OK) 
      {
        Serial.println("Message queued for transmission");
      } 
      else 
      {
        Serial.println("Failed to queue message for transmission");
      }
    } 
  }
}

void BaseStationCore::start_receive_to_rx_queue_task(void* _this) {
  ((BaseStationCore*)_this)->receive_to_rx_queue();
}

void BaseStationCore::receive_to_rx_queue() {

  twai_message_t message;

  Serial.println("Receive to rx queue task created");

  while(1){
    // Check if alert happened
    uint32_t alerts_triggered;
    twai_read_alerts(&alerts_triggered, 0);
    twai_status_info_t twaistatus;
    twai_get_status_info(&twaistatus);

    // Handle alerts
    if (alerts_triggered & TWAI_ALERT_ERR_PASS) {
      Serial.println("Alert: TWAI controller has become error passive.");
    }
    if (alerts_triggered & TWAI_ALERT_BUS_ERROR) {
      Serial.println("Alert: A (Bit, Stuff, CRC, Form, ACK) error has occurred on the bus.");
      Serial.printf("Bus error count: %d\n", twaistatus.bus_error_count);

    }
    if (alerts_triggered & TWAI_ALERT_RX_QUEUE_FULL) {
      Serial.println("Alert: The RX queue is full causing a received frame to be lost.");
      Serial.printf("RX buffered: %d\t", twaistatus.msgs_to_rx);
      Serial.printf("RX missed: %d\t", twaistatus.rx_missed_count);
      Serial.printf("RX overrun %d\n", twaistatus.rx_overrun_count);
    }

    if (alerts_triggered & TWAI_ALERT_BUS_OFF) {
      Serial.println("Bus Off state");
    }

    bool receive = false;

    //TODO: Not sure if this do while loop for receiving messages is the best way to do it, but seems to be working right now
    do {
      //Try to receive a message
      receive = twai_receive(&message, RX_TX_BLOCK_TIME);
      if (receive == ESP_OK) {
        //If message is received succesfully, put it in the rx_queue
        xQueueSend(this->rx_queue, &message, 0);
      } else if(receive == ESP_ERR_INVALID_STATE || receive == ESP_ERR_INVALID_ARG) {
        Serial.println("Failed to receive message");
      }
    } while (receive != ESP_ERR_TIMEOUT); //If the message is not received, try again, not sure if this is the best way to do it
  }
}

void BaseStationCore::start_rx_queue_event_task(void* _this) {
  ((BaseStationCore*)_this)->rx_queue_event();
}

void BaseStationCore::rx_queue_event() {
  twai_message_t message;

  while(1){
    if(xQueueReceive(rx_queue, &message, RX_TX_BLOCK_TIME) == pdTRUE){
      //If there is a message in the rx_queue
      uint64_t data = 0;
      uint8_t nodeID = 0;
      uint16_t messageID = 0;

      //Extract the data from the message data
      Serial.println("Message received with data: " + String(*message.data));
      memcpy(&data, message.data, 8);

      //Extract the nodeID and messageID from the message identifier with format:
      //8bit node ID, 16bit message ID, 5bit reserved
		  //NNMMMMRR
      //(NNNN)(NNNN)(MMMM)(MMMM)(MMMM)(MMMM)(RRRR)(R000)
      nodeID = message.identifier >> 21;
      messageID = message.identifier >> 5;

      Serial.print("Message received: ");
      Serial.print("Node ID: ");
      Serial.print(nodeID, HEX);
      Serial.print(" Message ID: ");
      Serial.print(messageID, HEX);
      Serial.print(" Data: ");
      Serial.println(data, HEX);

      //Call the onMessageReceived function in the device code
      this->onMessageReceived(nodeID,messageID, data);
    }
  }
}

void BaseStationCore::sendMessage(uint16_t messageID, uint64_t *data) {
  //Create a CAN Bus ID with format:
  //8bit node ID, 16bit message ID, 5bit reserved
	//NNMMMMRR
  //(NNNN)(NNNN)(MMMM)(MMMM)(MMMM)(MMMM)(RRRR)(R000)
  uint32_t id = 0; 
  id = ((this->nodeID << 24) | (messageID << 8)) >> 3;

  Serial.print("Sending message with Node ID: ");
  Serial.print(nodeID, HEX);
  Serial.print(" Message ID: ");
  Serial.print(messageID, HEX);
  Serial.print(" Data: ");
  Serial.println(*data, HEX);
  //Create a CAN Bus message
  twai_message_t message = create_message(id, data);
  //Send the message to the tx_queue
  xQueueSend(tx_queue, &message, portMAX_DELAY);
}
void BaseStationCore::sendMessage(uint16_t messageID, float *data){
  uint64_t data64 = 0;
  memcpy(&data64, data, 4);
  sendMessage(messageID, &data64);
}