#include "Arduino.h"
#include <SPI.h>
#define TRANSFER_WAIT 16
#define ACT_WAIT 2500

//Memory Card Responses
//0x47 - Good
//0x4E - BadChecksum
//0xFF - BadSector

//Define pins
#define interruptPin 2          //Attention (Select)
#define AttPin1 3          //Attention (Select)
#define AttPin2 4          //Attention (Select)

//いじると壊れる
bool use_log = true;

byte memory_card_buf[128];
//byte memory_card_buf_temp[128][32];
byte controller_buf[4];

//JVS
bool noack = false;
bool init_jvs = false;
byte request[256];
byte answer[256];
byte send_data[256];
bool initDone;
bool escByte;
int answer_count = 0;
int req_i;
int req_index = 0;

volatile byte device_id[48] = {0x2F, 0x4B, 0x4F, 0x4E, 0x41, 0x4D, 0x49, 0x20, 0x43, 0x4F, 0x2E, 0x2C, 0x4C, 0x54, 0x44, 0x2E, 0x3B, 0x57, 0x68, 0x69, 0x74, 0x65, 0x20, 0x49, 0x2F, 0x4F, 0x3B, 0x56, 0x65, 0x72, 0x31, 0x2E, 0x30, 0x3B, 0x57, 0x68, 0x69, 0x74, 0x65, 0x20, 0x49, 0x2F, 0x4F, 0x20, 0x50, 0x43, 0x42, 0x00};
volatile byte jvs_command_info[2] = {0x01, 0x11};
volatile byte jvs_version_info[2] = {0x01, 0x20};
volatile byte jvs_trans_info[2] = {0x01, 0x20};
volatile byte jvs_function_info[2] = {0x01, 0x00};

volatile byte command_OK1[1] = {0x00};
volatile byte command_OK2[2] = {0x01, 0x01};
//sec
volatile byte sec_plate_reg[5] = {0xFF, 0xFF, 0xAC, 0x09, 0x00};
volatile byte sec_plate0_data[5] = {0x4A, 0x42, 0x00, 0x00, 0x73};
volatile byte sec_plate1_data[5] = {0x4A, 0x43, 0x00, 0x00, 0x72};
byte sec_plate1_status = 0;
byte sec_plate2_status = 0;
byte select_slot = 0;
//mem
byte select_port = 2;

byte memory_card1_status1 = 0x00;
byte memory_card2_status1 = 0x00;
byte memory_card3_status1 = 0x00;

byte memory_card1_status2 = 0x08;
byte memory_card2_status2 = 0x08;
byte memory_card3_status2 = 0x00;
#define rs485_tx HIGH
#define rs485_rx LOW
#define mode_memory_card 0
#define mode_sec_plate 1
byte buf_mode = 0;
const int serialctl = 5;
const int jvs_sense = 6;

int base_address;
byte pcb_buf[128];
unsigned long time_start;
unsigned long time_end;

//memory_card_writer
bool is_pc_mode = false;
void PS_SLOT_PinSetup()
{
  SPI.setBitOrder(LSBFIRST);
  SPI.setClockDivider(SPI_CLOCK_DIV32);
  SPI.setDataMode(SPI_MODE3);
  SPI.begin();
  pinMode(MISO, OUTPUT);
  pinMode(AttPin1, OUTPUT);
  pinMode(AttPin2, OUTPUT);
  pinMode(interruptPin, INPUT);
}

void setup()
{
  memset(request, 0x00, 256);
  memset(answer, 0x00, 256);
  memset(pcb_buf, 0x00, 128);

  Serial.begin(115200);
  Serial3.begin(115200);

  pinMode(serialctl, OUTPUT);
  pinMode(jvs_sense, INPUT);
  digitalWrite(jvs_sense, LOW);
  digitalWrite(serialctl, rs485_rx);
  PS_SLOT_PinSetup();
  initDone = true;
  is_pc_mode = false;
  Serial.println("START");
  //Serial3.println("START");

}
void loop()
{
  while (1)
  {
    if (use_log == false && is_pc_mode == false)
    {
      Serial.println("Running...");
    }
    digitalWrite(13, LOW);
    serialEvent3_();
  }
}
void serialEvent3_()
{
  if (initDone)
  {
    getRequest();
  }
  if (isRequestComplete() == true)
  {
    digitalWrite(13, HIGH);
    if (checkRequestChecksum() == true )
    {
      processRequest();

      req_i = 0;
      memset(request, 0x00, 256);
    } else {
      req_i = 0;
    }
  }
}
void getRequest()
{
  byte inByte = 0x00;
  bool is_req_ok = false;
  if (Serial.available() > 0)
  {
    if (is_pc_mode == false) {
      Serial.println("PC_MODE");
    }
    is_pc_mode = true;
    is_req_ok = true;
    inByte = Serial.read();
  }

  if (Serial3.available() > 0) {
    is_pc_mode = false;
    is_req_ok = true;
    inByte = Serial3.read();
  }
  if (is_req_ok == true) {

   if (use_log == true && is_pc_mode == false) {
      if (inByte <= 0xF)
      {
        Serial.print("0");
      }
      Serial.print(inByte, HEX);
      Serial.print(" ");
    }
    if (inByte == 0xE0)
    {
      escByte = false;
      req_i = 0;
    }
    if (inByte == 0xD0)
    {
      escByte = true;
    }
    if (inByte != 0xE0 && inByte != 0xD0)
    {
      if (escByte == true)
      {
        inByte = inByte + 1;
        escByte = false;
      }
      request[req_i] = inByte;
      req_i++;
    }
  }
}
boolean isRequestComplete()
{
  if (req_i >= 4)
  {
    if (req_i == ( 2 + request[1]))
    {
      time_start = millis();
      if (use_log == true && is_pc_mode == false) {
        Serial.println("");
      }
      return true;
    }
  }
  return false;
}
boolean checkRequestChecksum()
{
  byte sum = 0;
  int bufsize = 2 + request[1];
  for (int i = 0; i < bufsize - 1; i++)
  {
    sum += request[i];
  }
  return (sum == request[bufsize - 1]);
}
void processRequest()
{
  memset(answer, 0x00, 256);
  processRequest_(request, answer);
  if (noack == false) {
    sendAnswer(answer);
  } else {
    noack = false;
  }
  if (init_jvs == true)
  {
    pinMode(jvs_sense, OUTPUT);
  }
}
void sendAnswer(byte* answer)
{
  byte sum = 0;
  int bufsize = 2 + answer[1];

  memset(send_data, 0x00, 256);
  for (int i = 0; i < bufsize - 1; i++)
  {
    sum += answer[i];
  }
  answer_count = 0;
  answer[bufsize - 1] = sum;
  send_data[0] = 0xE0;
  for (int i = 0; i < bufsize + 2; i++)
  {
    byte outByte =  answer[i];
    if ( outByte == 0xE0 || outByte == 0xD0 )
    {

      outByte = outByte - 1;
      send_data[answer_count + 1] = 0xD0;
      answer_count++;
    }
    send_data[answer_count + 1] = outByte;
    answer_count++;
  }

  rs485_send(send_data, answer_count - 1);

}
void sec_plate(byte* request, byte* answer)
{
  if (use_log == true && is_pc_mode == false) {
    Serial.print("SEC_PLATE_COMMAND::");
  }
  if (request[req_index + 1] != 0x40 && request[req_index + 1] != 0x20 && request[req_index + 1] != 0x10)
  {
    req_index = req_index + 2;
    if (use_log == true && is_pc_mode == false) {
      Serial.print("SLOT-");
    }
    select_slot = (byte)(request[req_index + 1] & 0x01);
    if (use_log == true && is_pc_mode == false) {
      Serial.println(select_slot, HEX);
    }
  }
  if (select_slot == 0x00)
  {
    sec_plate1_status = 0x00;
  } else {
    sec_plate2_status = 0x00;
  }
  if (request[req_index + 1] == 0x10)
  {
    req_index = req_index + 10;
    if (use_log == true && is_pc_mode == false) {
      Serial.println("SET_PASS::");
    }
  }
  if (request[req_index + 1] == 0x20)
  {
    req_index = req_index + 9;
    memset(pcb_buf, 0x00, 128);
    buf_mode = mode_sec_plate;
    if (select_slot == 0x00)
    {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("TRANS_DATA_0::");
      }
      memcpy(pcb_buf, sec_plate0_data, 5);
    } else
    {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("TRANS_DATA_1::");
      }
      memcpy(pcb_buf, sec_plate1_data, 5);
    }
  }
  if (request[req_index + 1] == 0x40)
  {
    req_index = req_index + 5;
    if (use_log == true && is_pc_mode == false) {
      Serial.println("TRANS_REG::");
    }
    buf_mode = mode_sec_plate;
    memset(pcb_buf, 0x00, 128);
    memcpy(pcb_buf, sec_plate_reg, 5);
  }
  make_responce(answer, command_OK1);
}
void control_buf(byte* request, byte* answer)
{
  if (use_log == true && is_pc_mode == false) {
    Serial.print("CONTROL_BUF::");
  }
  use_log = false;
  int offset = ((request[req_index + 3] & 0x7F) << 1) + ((request[req_index + 4] & 0x80) >> 7);
  int len = request[req_index + 5];
  if (request[req_index + 1] == 0)
  {
    if (buf_mode == mode_memory_card)
    {
      if (use_log == true && is_pc_mode == false) {
        Serial.print("MODE:MEMORY_CARD::");
      }
      memset(memory_card_buf, 0x00, 128);
      if (PS_SLOT_find_slot_memory_card(select_port) == 1)
      {
        if (use_log == true && is_pc_mode == false) {
          Serial.println("CARD_FIND::");
        }
        if (PS_SLOT_ReadFrame(select_port, base_address + offset) == 0x47) {

          if (use_log == true && is_pc_mode == false) {
            Serial.println("READ_OK");
          }
          memset(pcb_buf, 0x00, 128);
          memcpy(pcb_buf, memory_card_buf, 128);
        } else
        {
          if (use_log == true && is_pc_mode == false) {
            Serial.println("READ_NG");
          }
        }
      } else {
        if (use_log == true && is_pc_mode == false) {
          Serial.println("NO_INSERT");
        }
      }
    }

    if (use_log == true && is_pc_mode == false) {
      Serial.print("PCB_BUF_READ::base_address=");
      Serial.print(base_address, HEX);
      Serial.print(":offset=");
      Serial.print(offset, HEX);
      Serial.print(":LEN=");
      Serial.print(len, HEX);
      Serial.print(":port=");
      Serial.print(select_port, HEX);
      Serial.print(":MODE=");
      Serial.println(buf_mode, HEX);
    }

    byte buf_data[128 + 1];
    buf_data[0] = len;
    memcpy(buf_data + 1, pcb_buf, len);
    req_index = req_index + 6;
    make_responce(answer, buf_data);
  }
  if (request[req_index + 1] == 1)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.print("PCB_BUF_WRITE::base_address=");
      Serial.print(base_address, HEX);
      Serial.print(":LEN=");
      Serial.println(len, HEX);
    }
    memset(pcb_buf, 0x00, 128);
    for (int i = 0; i < len; i++) {
      pcb_buf[i] = request[(req_index + 6 + i)];
    }
    req_index = req_index + 6 + len;
    make_responce(answer, command_OK1);
  }
  if (request[req_index + 1] == 2)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("PCB_BUF_UNKNOWN::");
    }
    req_index = req_index + 5;
    make_responce(answer, command_OK1);
  }

  use_log = true;

}
void memory_card_function(byte* request, byte* answer)
{
  if (request[req_index + 1] == 0x74)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("MEMORY_CARD_READ_TRANS_BUF::");
    }
    buf_mode = mode_memory_card;
    if ((request[req_index + 2] & 0xF0) == 0x00) {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("PORT-00::");
      }
      select_port = 0;
    } else {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("PORT-01::");
      }
      select_port = 1;
    }

    byte base_address_H = (request[req_index + 2] & 0x0F);
    byte base_address_L = request[req_index + 3];

    base_address = (base_address_H << 8) + base_address_L;
    if (use_log == true && is_pc_mode == false) {
      Serial.print("base_address::");
      Serial.print(base_address, HEX);
      Serial.print(":");
    }
    if (PS_SLOT_find_slot_memory_card(select_port) == 1)
    {
      if (PS_SLOT_ReadFrame(select_port, base_address) == 0x47) {
        memset(pcb_buf, 0x00, 128);
        memcpy(pcb_buf, memory_card_buf, 128);

        if (use_log == true && is_pc_mode == false) {
          Serial.println("state::READ_OK");
        }
        if (select_port == 0) {
          memory_card1_status1 = 0x80;
          memory_card1_status2 = 0x00;
        } else
        {
          memory_card2_status1 = 0x80;
          memory_card2_status2 = 0x00;
        }
      } else
      {
        if (use_log == true && is_pc_mode == false) {
          Serial.println("state::READ_NG");
        }
        if (select_port == 0) {
          memory_card1_status1 = 0x00;
          memory_card1_status2 = 0x00;
        } else
        {
          memory_card2_status1 = 0x00;
          memory_card2_status2 = 0x00;
        }
      }

    } else {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("state::NO_INSERT");
      }
      if (select_port == 0) {
        memory_card1_status1 = 0x00;
        memory_card1_status2 = 0x08;
      } else
      {
        memory_card2_status1 = 0x00;
        memory_card2_status2 = 0x08;
      }
    }
  }
  if (request[req_index + 1] == 0x75)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("MEMORY_CARD_WRITE::");
    }
    if ((request[req_index + 5] & 0xF0) == 0x00) {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("PORT-00::");
      }
      select_port = 0;
    } else {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("PORT-01::");
      }
      select_port = 1;
    }
    byte base_address_H = (byte)(request[req_index + 5] & 0x0F);
    byte base_address_L = request[req_index + 6];
    int address = (base_address_H << 8) + base_address_L;
    if (use_log == true && is_pc_mode == false) {
      Serial.print("write_address::");
      Serial.print(address, HEX);
      Serial.print(":");
    }
    memset(memory_card_buf, 0x00, 128);
    memcpy(memory_card_buf, pcb_buf, 128);
    if (PS_SLOT_WriteFrame(select_port, address) == 0x47)
    {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("state::WRITE_OK");
      }
      if (select_port == 0) {
        memory_card1_status1 = 0x80;
        memory_card1_status2 = 0x00;
      } else
      {
        memory_card2_status1 = 0x80;
        memory_card2_status2 = 0x00;
      }
    } else
    {
      if (use_log == true && is_pc_mode == false) {
        Serial.println("state::WRITE_NG");
      }
      if (select_port == 0) {
        memory_card1_status1 = 0x00;
        memory_card1_status2 = 0x00;
      } else
      {
        memory_card2_status1 = 0x00;
        memory_card2_status2 = 0x00;
      }
    }
  }
  req_index = req_index + 9;
  make_responce(answer, command_OK2);
}


void make_status(byte* request, byte* answer)
{
  if (use_log == true && is_pc_mode == false) {
    Serial.print("STATUS_CHECK::");
  }
  byte status1 = 0x00;
  byte status2 = 0x00;


  if (select_port == 0)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("PORT-00::");
    }
    status1 = memory_card1_status1;
    status2 = memory_card1_status2;
  } if (select_port == 1)
  {

    if (use_log == true && is_pc_mode == false) {
      Serial.println("PORT-01::");
    }
    status1 = memory_card2_status1;
    status2 = memory_card2_status2;
  } if (select_port == 2)
  {

    if (use_log == true && is_pc_mode == false) {
      Serial.println("PORT---");
    }
    status1 = memory_card3_status1;
    status2 = memory_card3_status2;
  }
  if (select_slot == 0)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("slot-00::");
    }
    status2 = status2 | sec_plate1_status;
  } if (select_slot == 1)
  {
    if (use_log == true && is_pc_mode == false) {
      Serial.println("slot-01::");
    }
    status2 = status2 | sec_plate2_status;
  }
  byte state[3];
  state[0] = 0x02;
  state[1] = status1;
  state[2] = status2;
  req_index = req_index + 1;
  make_responce(answer, state);
}
void make_responce(byte* answer, byte* res)
{
  int index = answer[1];
  answer[index + 2] = 0x01;
  answer[1] = answer[1] + res[0] + 1;
  for (int i = 0; i < res[0]; i++)
  {
    answer[index + 3 + i] = res[i + 1];
  }
}
void processRequest_(byte* request, byte* answer)
{

  req_index = 0;
  memset(answer, 0x00, 256);
  answer[0] = 0;        // node id
  answer[1] = 1;        // res_count
  answer[2] = 1;        // status1

  req_index = 2;

  while (request[1] >= req_index)
  {
    switch (request[req_index])
    {
      case 0xF0:
        req_index = req_index + 2;
        if (use_log == true && is_pc_mode == false) {
          //Serial.println("JVS_RESET");
        }
        pinMode(jvs_sense, INPUT);

        select_slot = 0;
        select_port = 2;
        base_address = 0;
        noack = true;
        init_jvs = false;
        break;
      case 0xF1:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_SET_DEV_ID");
        }
        req_index = req_index + 2;
        make_responce(answer, command_OK1);
        base_address = 0;
        init_jvs = true;
        break;
      case 0x2F:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_RESEND");
        }
        req_index = req_index + 1;
        rs485_send(send_data, answer_count - 1);
        noack = true;
        break;
      case 0x10:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_GET_DEV_ID");
        }
        req_index = req_index + 1;
        make_responce(answer, device_id);
        break;
      case 0x11:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_GET_COMMAND_REV");
        }
        req_index = req_index + 1;
        make_responce(answer, jvs_command_info);
        break;
      case 0x12:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_GET_VERSION");
        }
        req_index = req_index + 1;
        make_responce(answer, jvs_version_info);
        break;
      case 0x13:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_GET_TRANS_VERSION");
        }
        req_index = req_index + 1;
        make_responce(answer, jvs_trans_info);
        break;
      case 0x14:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("JVS_GET_FUNCTION_INFO");
        }
        req_index = req_index + 1;
        make_responce(answer, jvs_function_info);
        break;
      case 0x70:
        control_buf(request, answer);
        break;
      case 0x71:
        make_status(request, answer);
        break;
      case 0x72:
        sec_plate(request, answer);
        break;
      case 0x73:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("PCB_FW_WRITE_DONE");
        }
        req_index = req_index + 1;
        make_responce(answer, command_OK1);
      case 0x76:
        memory_card_function(request, answer);
        break;
      case 0x77:
        if (use_log == true && is_pc_mode == false) {
          Serial.println("PS_CONTROLLER_GET");
        }
        req_index = req_index + 1;
        PS_SLOT_update_controller_buf();
        byte ps_data[5];
        ps_data[0] = 4;
        ps_data[1] = controller_buf[0];
        ps_data[2] = controller_buf[1];
        ps_data[3] = controller_buf[2];
        ps_data[4] = controller_buf[3];
        make_responce(answer, ps_data);
        break;
      default:
        req_index = req_index + 256;
        if (use_log == true && is_pc_mode == false) {
          Serial.println("UNKNOWN");
        }
        byte null_data[1];
        null_data[0] = 0x00;
        make_responce(answer, null_data);
        break;
    }
  }

  answer[1] = answer[1] + 1;            // data_size + sum

}

byte PS_SLOT_SendCommand(byte CommandByte)
{
  SPDR = CommandByte;                 //Start the transmission
  while (!(SPSR & (1 << SPIF)));      //Wait for the end of the transmission
  delayMicroseconds(16);
  return SPDR;
}

//Read a frame from Memory Card and send it to serial port
int PS_SLOT_ReadFrame(int slot, unsigned int Address)
{
  byte AddressLSB = Address & 0xFF;
  byte AddressMSB = (Address >> 8) & 0xFF;
  memset(memory_card_buf, 0x00, 128);
  byte command[30];
  unsigned long time;
  int i = 0;
  time = millis();
  if (slot == 0) {
    digitalWrite(AttPin1, LOW);
  } else
  {
    digitalWrite(AttPin2, LOW);
  }
  delayMicroseconds(TRANSFER_WAIT);
  command[0] = PS_SLOT_SendCommand(0x81);    //Access Memory Card
  command[1] = PS_SLOT_SendCommand(0x52);    //Send read command
  command[2] = PS_SLOT_SendCommand(0x00);    //Memory Card ID1
  command[3] = PS_SLOT_SendCommand(0x00);    //Memory Card ID2
  command[4] = PS_SLOT_SendCommand(AddressMSB);    //Address MSB
  command[5] = PS_SLOT_SendCommand(AddressLSB);    //Address LSB
  command[6] = PS_SLOT_SendCommand(0x00);    //Memory Card ACK1
  command[7] = PS_SLOT_SendCommand(0x00);    //Memory Card ACK2
  delayMicroseconds(ACT_WAIT);
  command[8] = PS_SLOT_SendCommand(0x00);    //Confirm MSB
  command[9] = PS_SLOT_SendCommand(0x00);    //Confirm LSB
  //Get 128 byte data from the frame
  for (i = 0; i < 128; i++)
  {
    memory_card_buf[i] = PS_SLOT_SendCommand(0x00);
  }
  command[10] = PS_SLOT_SendCommand(0x00);   //Checksum (MSB xor LSB xor Data)
  command[11] = PS_SLOT_SendCommand(0x00);   //Memory Card status byte


  //Deactivate device

  if (slot == 0) {
    digitalWrite(AttPin1, HIGH);
  } else
  {
    digitalWrite(AttPin2, HIGH);
  }
  return (int)command[11];
}

//Write a frame from the serial port to the Memory Card
int PS_SLOT_WriteFrame(int slot, unsigned int Address)
{
  int ret = 0;
  byte checksum;
  byte AddressMSB = (Address >> 8) & 0xFF;
  byte AddressLSB = Address & 0xFF;

  byte checksum_data = 0;
  int DelayCounter = 30;
  //memset(ReadData, 0xFF, 128);
  
  if (slot == 0) {
    digitalWrite(AttPin1, LOW);
  } else
  {

    digitalWrite(AttPin2, LOW);
  }

  delayMicroseconds(TRANSFER_WAIT);
  PS_SLOT_SendCommand(0x81);      //Access Memory Card
  PS_SLOT_SendCommand(0x57);      //Send write command
  PS_SLOT_SendCommand(0x00);      //Memory Card ID1
  PS_SLOT_SendCommand(0x00);      //Memory Card ID2
  PS_SLOT_SendCommand(AddressMSB);      //Address MSB
  PS_SLOT_SendCommand(AddressLSB);      //Address LSB

  delayMicroseconds(ACT_WAIT);

  //Write 128 byte data to the frame
  for (int i = 0; i < 128; i++)
  {
    PS_SLOT_SendCommand(memory_card_buf[i]);
    checksum_data ^= memory_card_buf[i];
  }
  checksum = AddressMSB ^ AddressLSB ^ checksum_data;
  PS_SLOT_SendCommand(checksum);      //Checksum (MSB xor LSB xor Data)
  PS_SLOT_SendCommand(0x00);               //Memory Card ACK1
  PS_SLOT_SendCommand(0x00);               //Memory Card ACK2
  ret = (int)PS_SLOT_SendCommand(0x00); //Memory Card status byte
  if (slot == 0) {
    digitalWrite(AttPin1, HIGH);
  } else
  {
    digitalWrite(AttPin2, HIGH);
  }
  return ret;
}
void PS_SLOT_update_controller_buf()
{
  byte command[5];
  memset(controller_buf, 0x00, 4);
  memset(command, 0x00, 5);
  digitalWrite(AttPin1, LOW);
  delayMicroseconds(TRANSFER_WAIT);
  command[0] = PS_SLOT_SendCommand(0x01);
  command[1] = PS_SLOT_SendCommand(0x42);
  command[2] = PS_SLOT_SendCommand(0x00);
  command[3] = PS_SLOT_SendCommand(0x00);
  command[4] = PS_SLOT_SendCommand(0x00);
  digitalWrite(AttPin1, HIGH);
  if (command[1] == 0x41 && command[2] == 0x5A)
  {
    controller_buf[0] = command[3];
    controller_buf[1] = command[4];
  }
  memset(command, 0x00, 5);
  digitalWrite(AttPin2, LOW);
  delayMicroseconds(TRANSFER_WAIT);
  command[0] = PS_SLOT_SendCommand(0x01);
  command[1] = PS_SLOT_SendCommand(0x42);
  command[2] = PS_SLOT_SendCommand(0x00);
  command[3] = PS_SLOT_SendCommand(0x00);
  command[4] = PS_SLOT_SendCommand(0x00);
  digitalWrite(AttPin2, HIGH);
  if (command[1] == 0x41 && command[2] == 0x5A)
  {
    controller_buf[2] = command[3];
    controller_buf[3] = command[4];
  }

}

int PS_SLOT_find_slot_memory_card(int slot)
{
  int ret = 0;
  byte command[4];
  if (slot == 0) {
    digitalWrite(AttPin1, LOW);
  } else
  {
    digitalWrite(AttPin2, LOW);
  }
  delayMicroseconds(TRANSFER_WAIT);
  command[0] = PS_SLOT_SendCommand(0x81);    //Access Memory Card
  command[1] = PS_SLOT_SendCommand(0x52);    //Send read command
  command[2] = PS_SLOT_SendCommand(0x00);    //Memory Card ID1
  command[3] = PS_SLOT_SendCommand(0x00);    //Memory Card ID2

  if (slot == 0) {
    digitalWrite(AttPin1, HIGH);
  } else
  {
    digitalWrite(AttPin2, HIGH);
  }
  if (command[2] == 0x5A)
  {
    ret = 01;
  }
  return ret;
}


void rs485_send(const byte *addr, byte len)
{
  time_end = millis();
  digitalWrite(serialctl, rs485_tx);
  if (is_pc_mode == true) {
    Serial.write(addr, len);
  } else {
    Serial3.write(addr, len);
    while (!(UCSR3A & (1 << UDRE3)))  // Wait for empty transmit buffer
      UCSR3A |= 1 << TXC3;  // mark transmission not complete
    while (!(UCSR3A & (1 << TXC3)));   // Wait for the transmission to complete
  }
  digitalWrite(serialctl, rs485_rx);

  if (use_log == true && is_pc_mode == false) {
    Serial.print(time_end - time_start, DEC);
    Serial.println("");
    for (int i = 0; i < len; i++)
    {
      if (addr[i] < 0x10) {
        Serial.print("0");
      }
      Serial.print(addr[i], HEX);
      Serial.print(":");
    }
    Serial.println("");
    Serial.println("");
  }
}
