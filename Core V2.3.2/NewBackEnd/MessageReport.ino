/*

Message Report

This task handles sending plaintext to the server as debug messages.

Global Variables Used:
SendMessage: bool, other tasks can set it to 1 to indicate they need to send a message and to reserve the global variables. Set back to 0 when message sent properly.
Message: String, the message to be sent.
ReadyToSend: bool, indicates the other task has finished writing its message and it is ok to send.

*/

void MessageReport(void *pvParameters) {
  while(1){
    if(!ReadyToSend){
      //No message ready to send
      continue;
    }
  }
}