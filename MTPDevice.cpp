/* USB EHCI Host for Teensy 3.6
 * Copyright 2017 Paul Stoffregen (paul@pjrc.com)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <Arduino.h>
#include "USBHost_t36.h"  // Read this header first for key info
#include "MTPDevice.h"

#define print   USBHost::print_
#define println USBHost::println_

#define DEBUG_MTP
//#define DEBUG_MTP_VERBOSE

#ifndef DEBUG_MTP
#undef DEBUG_MTP_VERBOSE
void inline DBGPrintf(...) {};
#else
#define DBGPrintf USBHDBGSerial.printf
#endif

#ifndef DEBUG_MTP_VERBOSE
void inline VDBGPrintf(...) {};
#else
#define VDBGPrintf USBHDBGSerial.printf
#endif


typedef struct {
  uint16_t  idVendor;   // vendor id of keyboard
  uint16_t  idProduct;    // product id - 0 implies all of the ones from vendor;
} known_mtp_devices_t;      // list of devices to hack in...

const static known_mtp_devices_t known_mtp_devices[] PROGMEM = {
  {0x1949, 0x000C}

};

void MTPDevice::init()
{
  contribute_Pipes(mypipes, sizeof(mypipes) / sizeof(Pipe_t));
  contribute_Transfers(mytransfers, sizeof(mytransfers) / sizeof(Transfer_t));
  contribute_String_Buffers(mystring_bufs, sizeof(mystring_bufs) / sizeof(strbuf_t));
  driver_ready_for_device(this);
}

bool MTPDevice::claim(Device_t *dev, int type, const uint8_t *descriptors, uint32_t len)
{
  println("MTPDevice claim this=", (uint32_t)this, HEX);
  print_hexbytes(descriptors, len);

  print("vid=", dev->idVendor, HEX);
  print(", pid=", dev->idProduct, HEX);
  print(", bDeviceClass = ", dev->bDeviceClass);
  print(", bDeviceSubClass = ", dev->bDeviceSubClass);
  println(", bDeviceProtocol = ", dev->bDeviceProtocol);

  // only claim at interface level
  if (type != 1) return false;
  if (len < 9 + 7 + 7 + 7) return false;

  uint32_t numendpoint = descriptors[4];
  if (numendpoint < 3) return false;    // Expecting RX, TX, and Event end points

  // Note many devices don't follow the rules... as mentioned in https://elinux.org/images/8/85/Media_Transfer_Protocol.pdf
  // so short cut for experiment of not doing a lot, but instead have table of known MTP devices as well
  // First see if device is marked for MTP...

  if ((descriptors[5] != 6) ||  // bInterfaceClass, 6 still image
      (descriptors[6] != 1) ||    // bInterfaceSubClass, 1
      (descriptors[7] != 1) ) { // bInterfaceProtocol, 1

    // See if we find it in list of known MTP devices
    bool found = false;
    for (uint8_t i = 0; i < sizeof(known_mtp_devices) / sizeof(known_mtp_devices[0]); i++ ) {
      if ((dev->idVendor == known_mtp_devices[i].idVendor) &&
          ((known_mtp_devices[i].idProduct == 0) || (known_mtp_devices[i].idProduct == dev->idProduct))) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  const uint8_t *p = descriptors;
  const uint8_t *end = p + len;

  p += 9;

  println("  Looks Like MTP interface");
  uint8_t rx_ep = 0;
  uint8_t tx_ep = 0;
  uint8_t event_ep = 0;
  rx_size_ = 0;
  tx_size_ = 0;
  event_size_ = 0;
  //uint32_t rx_interval = 0;
  //uint32_t tx_interval = 0;
  uint32_t event_interval = 0;

  while (p < end) {
    len = *p;
    if (len < 4) return false;
    if (p + len > end) return false; // reject if beyond end of data
    if ( p[1] == 5) {
      // endpoint descriptor
      if (p[0] < 7) return false; // at least 7 bytes
      println("     Endpoint: ", p[2], HEX);
      if (p[3] == 2) {  // Bulk ones should be the RX and TX
        if ((p[2] & 0xF0) == 0x80) {
          rx_ep = p[2] & 0x0F;
          rx_size_ = p[4] | (p[5] << 8);
          //rx_interval = p[6];
          println("      rx_size_ = ", rx_size_);
        } else {
          tx_ep = p[2];
          tx_size_ = p[4] | (p[5] << 8);
          //tx_interval = p[6];
          println("      tx_size_ = ", tx_size_);
        }
      } else if ((p[3] == 3) && ((p[2] & 0xF0) == 0x80)) {
        event_ep = p[2] & 0x0F;
        event_size_ = p[4] | (p[5] << 8);
        event_interval = p[6];
        println("      event_size_ = ", event_size_);

      }
    }
    p += len;
  }
  print("  exited loop rx:", rx_ep);
  print(", tx:", tx_ep);
  println(", event:", event_ep);

  if (!rx_ep || !tx_ep) return false;   // did not get our two end points

//  if (!init_buffers(rx_size_, tx_size_)) return false;

  rxpipe_ = new_Pipe(dev, 2, rx_ep, 1, rx_size_);
  if (!rxpipe_) return false;
  txpipe_ = new_Pipe(dev, 2, tx_ep, 0, tx_size_);
  if (!txpipe_) {
    // TODO: free rxpipe
    return false;
  }
  eventpipe_ = new_Pipe(dev, 3, event_ep, 1,  event_size_, event_interval);

  rxpipe_->callback_function = rx_callback;
  txpipe_->callback_function = tx_callback;
  eventpipe_->callback_function = event_callback;

  queue_Data_Transfer(rxpipe_, rx1, rx_size_, this);
  queue_Data_Transfer(rxpipe_, rx2, rx_size_, this);
  queue_Data_Transfer(eventpipe_, rxevent, event_size_, this);

  sendMsg(MTP_OPERATION_GET_DEVICE_INFO);
  setup_complete_ = false;
  return true;
}


void MTPDevice::control(const Transfer_t *transfer)
{
  println("control callback (MTP)");
  //control_queued = false;
  print_hexbytes(transfer->buffer, transfer->length);
  // To decode hex dump to human readable HID report summary:
  //   http://eleccelerator.com/usbdescreqparser/
  uint32_t mesg = transfer->setup.word1;
  println("  mesg = ", mesg, HEX);
}

void MTPDevice::callback(const Transfer_t *transfer)
{
  //println("MTPDevice Callback (static)");
//  if (transfer->driver) {
//    ((MTPDevice *)(transfer->driver))->new_data(transfer);
//  }
}

void MTPDevice::disconnect()
{
  // Lets clean out all of the nodes that we have allocated memory for.

  // Free up any old items that were not reused...
  // May need to recurse if these have children?
  for (uint8_t i = 0; i < cnt_storages_; i++) {
    if (storage_info_[i].storage.name) {extmem_free(storage_info_[i].storage.name); storage_info_[i].storage.name = nullptr;}
    if (storage_info_[i].volume_id) {extmem_free(storage_info_[i].volume_id); storage_info_[i].volume_id = nullptr;}
    freeStorageListTree(storage_info_[i].storage.child);
    storage_info_[i].storage.child = nullptr; // make sure to only do once.
  }

  if (device_friendly_name_) {
    extmem_free(device_friendly_name_);
    device_friendly_name_ = nullptr;
  }

  // Clear out all of the state information
  cnt_storages_ = 0;
  get_store_info_index_ = 0xff;
  cnt_object_formats_ = 0;
  cnt_object_property_ids_ = 0;

  setup_complete_ = false;
  enum_node_ = nullptr;
  prop_node_ = nullptr;
  prop_index_ = 0;

}

void MTPDevice::printNodeListItem(storage_list_t *item, uint8_t level) {
  while (item) {
    Serial.printf("%08x ", (uint32_t)item);
    for (uint8_t i = 0; i < level; i++) Serial.printf("  ");
    Serial.printf("ID:%08x P:%04x C:%04x: S:%08x F:%04x MD: %s %s\n", item->id, item->parent, item->child,
                  item->storage_id, item->format, (uint8_t*)item->modify_date, item->name);
    if (item->child) printNodeListItem(item->child, level + 1);
    item = item->next;
  }
}


void MTPDevice::printNodeList() {
  for (uint8_t i = 0; i < cnt_storages_; i++) printNodeListItem(&storage_info_[i].storage, 0);
}

void MTPDevice::freeStorageListTree(storage_list_t *item) {
  // Unwind the next recursion.
  while (item) {
    Serial.printf("## FreeStorageListTree: %08x %x %x: %s\n", (uint32_t)item, item->id, item->storage_id, item->name);
    storage_list_t *next = item->next;
    // This recurses will unwind one direction of the recursing.
    if (item->child) freeStorageListTree(item->child);
    if (item->name) extmem_free(item->name);
    extmem_free(item);
    item = next;
  }
}

bool MTPDevice::startEnumStorageIndex(uint8_t index) {
  if (index >= cnt_storages_) return false;

  enum_node_ = &storage_info_[index].storage;
  sendMsg(MTP_OPERATION_GET_OBJECT_HANDLES, enum_node_->id, 0, 0xffffffff);
  return true;
}

void MTPDevice::startEnumStorageNode(const storage_list_t *node) {
  if (node == nullptr) return;
  // Assuming not top level Will experiment.
  sendMsg(MTP_OPERATION_GET_OBJECT_HANDLES, node->storage_id, 0, (node->id == node->storage_id) ? 0xffffffff : node->id);
  enum_node_ = (storage_list_t*)node;
}

const MTPDevice::storage_list_t *MTPDevice::findStorageItem(storage_list_t *item, uint32_t id) {
  while (item) {
    if (item->id == id) return item;
    if (item->child) {
      const storage_list_t *found_item = findStorageItem(item->child, id);
      if (found_item) return found_item;
    }
    item = item->next;
  }
  return nullptr;
}

const MTPDevice::storage_list_t *MTPDevice::findStorageItemByID(uint32_t id) {
  // bugbug:: this currently uses brute forcet o find it.
  for (uint8_t i = 0; i < cnt_storages_; i++) {
    if (storage_info_[i].storage.id == id) return &storage_info_[i].storage;
    const storage_list_t *found_item =  findStorageItem(storage_info_[i].storage.child, id);
    if (found_item) return found_item;
  }
  return nullptr;
}

// Experiment to see if we setup commands that we wait for completion.
uint32_t MTPDevice::deleteObject(uint32_t id, uint32_t format, uint32_t timeoutMS) {
  transaction_id_++;
  last_response_ = 0xffffffff;  // this should get overwritten.
  sendMsg(MTP_OPERATION_DELETE_OBJECT, id, format);
  elapsedMillis em = 0;
  while ((last_response_ == 0xffffffff) && (em < timeoutMS)) {
    USBHost::Task();  // maybe run the task for a bit of time
  }
  return last_response_;
}




void MTPDevice::sendMsg(uint16_t operation)
{
  MTPContainer *c = (MTPContainer *)txbuffer;
  c->len = 12;
  c->type = MTP_CONTAINER_TYPE_COMMAND;
  c->op = operation;
  c->transaction_id = transaction_id_;
#if defined(DEBUG_MTP)
  printContainer(c, "C-> ");
#endif
  queue_Data_Transfer(txpipe_, txbuffer, c->len, this);
  last_mtp_op_ = operation;
}

void MTPDevice::sendMsg(uint16_t operation, uint32_t p1)
{
  MTPContainer *c = (MTPContainer *)txbuffer;
  c->len = 16;
  c->type = MTP_CONTAINER_TYPE_COMMAND;
  c->op = operation;
  c->transaction_id = transaction_id_;
  c->params[0] = p1;
#if defined(DEBUG_MTP)
  printContainer(c, "C-> ");
#endif
  queue_Data_Transfer(txpipe_, txbuffer, c->len, this);
  last_mtp_op_ = operation;
}

void MTPDevice::sendMsg(uint16_t operation, uint32_t p1, uint32_t p2)
{
  MTPContainer *c = (MTPContainer *)txbuffer;
  c->len = 20;
  c->type = MTP_CONTAINER_TYPE_COMMAND;
  c->op = operation;
  c->transaction_id = transaction_id_;
  c->params[0] = p1;
  c->params[1] = p2;
#if defined(DEBUG_MTP)
  printContainer(c, "C-> ");
#endif
  queue_Data_Transfer(txpipe_, txbuffer, c->len, this);
  last_mtp_op_ = operation;
}

void MTPDevice::sendMsg(uint16_t operation, uint32_t p1, uint32_t p2, uint32_t p3)
{
  MTPContainer *c = (MTPContainer *)txbuffer;
  c->len = 24;
  c->type = MTP_CONTAINER_TYPE_COMMAND;
  c->op = operation;
  c->transaction_id = transaction_id_;
  c->params[0] = p1;
  c->params[1] = p2;
  c->params[2] = p3;
#if defined(DEBUG_MTP)
  printContainer(c, "C-> ");
#endif
  queue_Data_Transfer(txpipe_, txbuffer, c->len, this);
  last_mtp_op_ = operation;
}

void MTPDevice::sendFileObject(uint32_t storage, uint32_t parent, const char *name, File &file) {
  // 
  send_file_count_left_ = file.size();
  send_file_buffer_ptr_ = nullptr; // not ready yet
  if (send_file_buffer_) extmem_free(send_file_buffer_); // free previous if we have one

  send_file_buffer_ = (char*)extmem_malloc(send_file_count_left_);
  
  // quick and dirty read file into buffer.
  if (!send_file_buffer_) {
    // failed to allocate buffer. 
    Serial.printf("MTPDevice::sendFileObject failed to allocate buffer size: %u\n", send_file_count_left_);
    return;
  }
  file.read(send_file_buffer_, send_file_count_left_);
  file.close();

  ++transaction_id_;
  sendMsg(MTP_OPERATION_SEND_OBJECT_INFO, storage, parent);

  MTPHeader *c = (MTPHeader *)txbuffer2;
  uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);

  c->type = MTP_CONTAINER_TYPE_DATA;
  c->op = MTP_OPERATION_SEND_OBJECT_INFO;
  c->transaction_id = transaction_id_;

  write32(storage, &pdata); // storage
  write16(0x3000, &pdata); // format
  write16(0x0000, &pdata); // protection
  write32(send_file_count_left_, &pdata);  // size
  write16(0x0000,&pdata);  // thumb format
  write32(0x0000,&pdata);  // thumb size
  write32(0x0000,&pdata);  // thumb width
  write32(0x0000,&pdata);  // thumb height
  write32(0x0000,&pdata);  // pix width
  write32(0x0000,&pdata);  // pix height
  write32(0x0000,&pdata);  // bit depth
  write32(parent,&pdata);  // parent
  write16(0x0000,&pdata);  // association type
  write32(0x0000,&pdata);  // association description
  write32(0x0000,&pdata);  // sequence number
  writeStr(name, &pdata);  // object name
  writeStr("", &pdata);    // date Created
  writeStr("", &pdata);    // date modified
  writeStr("", &pdata);    // Keywords
  c->len = (uint32_t)pdata - (uint32_t)c;

  queue_Data_Transfer(txpipe_, txbuffer2, c->len, this);
#if defined(DEBUG_MTP)
  printContainer((MTPContainer *)c, "d-> ");
  print_hexbytes(txbuffer2, c->len);

#endif

}

void MTPDevice::sendObjectMsg(uint32_t storage, uint32_t parent, uint32_t object_id)
{

  if (!send_file_buffer_) {
    println("MTPDevice::sendObjectMsg called with no buffer active");
    return;
  }

  ++transaction_id_;

  MTPHeader *c = (MTPHeader *)txbuffer2;
  uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);

  c->type = MTP_CONTAINER_TYPE_DATA;
  c->op = MTP_OPERATION_SEND_OBJECT;
  c->transaction_id = transaction_id_;
  c->len = send_file_count_left_ + sizeof(MTPHeader);

  // now read in the first N bytes of the file.
  uint16_t cb_read = min (send_file_count_left_, tx_size_ - sizeof(MTPHeader));
  send_file_object_id_ = object_id; // not ready yet

  Serial.printf("sendObjectMsg %x\n", object_id); Serial.flush();

  send_file_buffer_ptr_ = send_file_buffer_; // setup pointer to first chunk of file
  memcpy(pdata, send_file_buffer_ptr_, cb_read);
  send_file_buffer_ptr_ += cb_read; // setup pointer to first chunk o file
  send_file_count_left_ -= cb_read;  // probably should check how many were actually read...
  if (send_file_count_left_ == 0) {
    extmem_free(send_file_buffer_);
    send_file_buffer_ = nullptr;
    send_file_buffer_ptr_ = nullptr; 
  }
  sendMsg(MTP_OPERATION_SEND_OBJECT);
  Serial.flush();
  queue_Data_Transfer(txpipe_, txbuffer2, cb_read + sizeof(MTPHeader), this);
}

void MTPDevice::printContainer(MTPContainer *c, const char *msg) {
  if (msg) Serial.printf("%s", msg);
  int print_property_name = -1;  //no
  switch (c->type) {
  default: print(" UNKWN:", c->type, HEX); break;
  case MTP_CONTAINER_TYPE_COMMAND: Serial.printf(F("CMD: ")); break;
  case MTP_CONTAINER_TYPE_DATA: Serial.printf(F("DATA:")); break;
  case MTP_CONTAINER_TYPE_RESPONSE: Serial.printf(F("RESP:")); break;
  case MTP_CONTAINER_TYPE_EVENT: Serial.printf(F("EVENT: ")); break;
  }
  Serial.printf(F("%x"), c->op);
  switch (c->op) {
  case MTP_OPERATION_GET_DEVICE_INFO: Serial.printf(F("(GET_DEVICE_INFO)")); break;
  case MTP_OPERATION_OPEN_SESSION: Serial.printf(F("(OPEN_SESSION)")); break;
  case MTP_OPERATION_CLOSE_SESSION: Serial.printf(F("(CLOSE_SESSION)")); break;
  case MTP_OPERATION_GET_STORAGE_IDS: Serial.printf(F("(GET_STORAGE_IDS)")); break;
  case MTP_OPERATION_GET_STORAGE_INFO: Serial.printf(F("(GET_STORAGE_INFO)")); break;
  case MTP_OPERATION_GET_NUM_OBJECTS: Serial.printf(F("(GET_NUM_OBJECTS)")); break;
  case MTP_OPERATION_GET_OBJECT_HANDLES: Serial.printf(F("(GET_OBJECT_HANDLES)")); break;
  case MTP_OPERATION_GET_OBJECT_INFO: Serial.printf(F("(GET_OBJECT_INFO)")); break;
  case MTP_OPERATION_GET_OBJECT: Serial.printf(F("(GET_OBJECT)")); break;
  case MTP_OPERATION_GET_THUMB: Serial.printf(F("(GET_THUMB)")); break;
  case MTP_OPERATION_DELETE_OBJECT: Serial.printf(F("(DELETE_OBJECT)")); break;
  case MTP_OPERATION_SEND_OBJECT_INFO: Serial.printf(F("(SEND_OBJECT_INFO)")); break;
  case MTP_OPERATION_SEND_OBJECT: Serial.printf(F("(SEND_OBJECT)")); break;
  case MTP_OPERATION_INITIATE_CAPTURE: Serial.printf(F("(INITIATE_CAPTURE)")); break;
  case MTP_OPERATION_FORMAT_STORE: Serial.printf(F("(FORMAT_STORE)")); break;
  case MTP_OPERATION_RESET_DEVICE: Serial.printf(F("(RESET_DEVICE)")); break;
  case MTP_OPERATION_SELF_TEST: Serial.printf(F("(SELF_TEST)")); break;
  case MTP_OPERATION_SET_OBJECT_PROTECTION: Serial.printf(F("(SET_OBJECT_PROTECTION)")); break;
  case MTP_OPERATION_POWER_DOWN: Serial.printf(F("(POWER_DOWN)")); break;
  case MTP_OPERATION_GET_DEVICE_PROP_DESC: Serial.printf(F("(GET_DEVICE_PROP_DESC)")); break;
  case MTP_OPERATION_GET_DEVICE_PROP_VALUE: Serial.printf(F("(GET_DEVICE_PROP_VALUE)")); break;
  case MTP_OPERATION_SET_DEVICE_PROP_VALUE: Serial.printf(F("(SET_DEVICE_PROP_VALUE)")); break;
  case MTP_OPERATION_RESET_DEVICE_PROP_VALUE: Serial.printf(F("(RESET_DEVICE_PROP_VALUE)")); break;
  case MTP_OPERATION_TERMINATE_OPEN_CAPTURE: Serial.printf(F("(TERMINATE_OPEN_CAPTURE)")); break;
  case MTP_OPERATION_MOVE_OBJECT: Serial.printf(F("(MOVE_OBJECT)")); break;
  case MTP_OPERATION_COPY_OBJECT: Serial.printf(F("(COPY_OBJECT)")); break;
  case MTP_OPERATION_GET_PARTIAL_OBJECT: Serial.printf(F("(GET_PARTIAL_OBJECT)")); break;
  case MTP_OPERATION_INITIATE_OPEN_CAPTURE: Serial.printf(F("(INITIATE_OPEN_CAPTURE)")); break;
  case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED: Serial.printf(F("(GET_OBJECT_PROPS_SUPPORTED)")); break;
  case MTP_OPERATION_GET_OBJECT_PROP_DESC: Serial.printf(F("(GET_OBJECT_PROP_DESC)")); print_property_name = 0; break;
  case MTP_OPERATION_GET_OBJECT_PROP_VALUE: Serial.printf(F("(GET_OBJECT_PROP_VALUE)")); print_property_name = 1; break;
  case MTP_OPERATION_SET_OBJECT_PROP_VALUE: Serial.printf(F("(SET_OBJECT_PROP_VALUE)")); break;
  case MTP_OPERATION_GET_OBJECT_PROP_LIST: Serial.printf(F("(GET_OBJECT_PROP_LIST)")); break;
  case MTP_OPERATION_SET_OBJECT_PROP_LIST: Serial.printf(F("(SET_OBJECT_PROP_LIST)")); break;
  case MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC: Serial.printf(F("(GET_INTERDEPENDENT_PROP_DESC)")); break;
  case MTP_OPERATION_SEND_OBJECT_PROP_LIST: Serial.printf(F("(SEND_OBJECT_PROP_LIST)")); break;
  case MTP_OPERATION_GET_OBJECT_REFERENCES: Serial.printf(F("(GET_OBJECT_REFERENCES)")); break;
  case MTP_OPERATION_SET_OBJECT_REFERENCES: Serial.printf(F("(SET_OBJECT_REFERENCES)")); break;
  case MTP_OPERATION_SKIP: Serial.printf(F("(SKIP)")); break;
  // RESPONSES
  case  MTP_RESPONSE_UNDEFINED: Serial.printf(F("(RSP:UNDEFINED)")); break;
  case  MTP_RESPONSE_OK:  Serial.printf(F("(RSP:OK)")); break;
  case  MTP_RESPONSE_GENERAL_ERROR: Serial.printf(F("(RSP:GENERAL_ERROR)")); break;
  case  MTP_RESPONSE_SESSION_NOT_OPEN:  Serial.printf(F("(RSP:SESSION_NOT_OPEN)")); break;
  case  MTP_RESPONSE_INVALID_TRANSACTION_ID:  Serial.printf(F("(RSP:INVALID_TRANSACTION_ID)")); break;
  case  MTP_RESPONSE_OPERATION_NOT_SUPPORTED: Serial.printf(F("(RSP:OPERATION_NOT_SUPPORTED)")); break;
  case  MTP_RESPONSE_PARAMETER_NOT_SUPPORTED: Serial.printf(F("(RSP:PARAMETER_NOT_SUPPORTED)")); break;
  case  MTP_RESPONSE_INCOMPLETE_TRANSFER: Serial.printf(F("(RSP:INCOMPLETE_TRANSFER)")); break;
  case  MTP_RESPONSE_INVALID_STORAGE_ID:  Serial.printf(F("(RSP:INVALID_STORAGE_ID)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_HANDLE: Serial.printf(F("(RSP:INVALID_OBJECT_HANDLE)")); break;
  case  MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED: Serial.printf(F("(RSP:DEVICE_PROP_NOT_SUPPORTED)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE:  Serial.printf(F("(RSP:INVALID_OBJECT_FORMAT_CODE)")); break;
  case  MTP_RESPONSE_STORAGE_FULL:  Serial.printf(F("(RSP:STORAGE_FULL)")); break;
  case  MTP_RESPONSE_OBJECT_WRITE_PROTECTED:  Serial.printf(F("(RSP:OBJECT_WRITE_PROTECTED)")); break;
  case  MTP_RESPONSE_STORE_READ_ONLY: Serial.printf(F("(RSP:STORE_READ_ONLY)")); break;
  case  MTP_RESPONSE_ACCESS_DENIED: Serial.printf(F("(RSP:ACCESS_DENIED)")); break;
  case  MTP_RESPONSE_NO_THUMBNAIL_PRESENT:  Serial.printf(F("(RSP:NO_THUMBNAIL_PRESENT)")); break;
  case  MTP_RESPONSE_SELF_TEST_FAILED:  Serial.printf(F("(RSP:SELF_TEST_FAILED)")); break;
  case  MTP_RESPONSE_PARTIAL_DELETION:  Serial.printf(F("(RSP:PARTIAL_DELETION)")); break;
  case  MTP_RESPONSE_STORE_NOT_AVAILABLE: Serial.printf(F("(RSP:STORE_NOT_AVAILABLE)")); break;
  case  MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED: Serial.printf(F("(RSP:SPECIFICATION_BY_FORMAT_UNSUPPORTED)")); break;
  case  MTP_RESPONSE_NO_VALID_OBJECT_INFO:  Serial.printf(F("(RSP:NO_VALID_OBJECT_INFO)")); break;
  case  MTP_RESPONSE_INVALID_CODE_FORMAT: Serial.printf(F("(RSP:INVALID_CODE_FORMAT)")); break;
  case  MTP_RESPONSE_UNKNOWN_VENDOR_CODE: Serial.printf(F("(RSP:UNKNOWN_VENDOR_CODE)")); break;
  case  MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED:  Serial.printf(F("(RSP:CAPTURE_ALREADY_TERMINATED)")); break;
  case  MTP_RESPONSE_DEVICE_BUSY: Serial.printf(F("(RSP:DEVICE_BUSY)")); break;
  case  MTP_RESPONSE_INVALID_PARENT_OBJECT: Serial.printf(F("(RSP:INVALID_PARENT_OBJECT)")); break;
  case  MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT:  Serial.printf(F("(RSP:INVALID_DEVICE_PROP_FORMAT)")); break;
  case  MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE: Serial.printf(F("(RSP:INVALID_DEVICE_PROP_VALUE)")); break;
  case  MTP_RESPONSE_INVALID_PARAMETER: Serial.printf(F("(RSP:INVALID_PARAMETER)")); break;
  case  MTP_RESPONSE_SESSION_ALREADY_OPEN:  Serial.printf(F("(RSP:SESSION_ALREADY_OPEN)")); break;
  case  MTP_RESPONSE_TRANSACTION_CANCELLED: Serial.printf(F("(RSP:TRANSACTION_CANCELLED)")); break;
  case  MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED:  Serial.printf(F("(RSP:SPECIFICATION_OF_DESTINATION_UNSUPPORTED)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_PROP_CODE:  Serial.printf(F("(RSP:INVALID_OBJECT_PROP_CODE)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT:  Serial.printf(F("(RSP:INVALID_OBJECT_PROP_FORMAT)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE: Serial.printf(F("(RSP:INVALID_OBJECT_PROP_VALUE)")); break;
  case  MTP_RESPONSE_INVALID_OBJECT_REFERENCE:  Serial.printf(F("(RSP:INVALID_OBJECT_REFERENCE)")); break;
  case  MTP_RESPONSE_GROUP_NOT_SUPPORTED: Serial.printf(F("(RSP:GROUP_NOT_SUPPORTED)")); break;
  case  MTP_RESPONSE_INVALID_DATASET: Serial.printf(F("(RSP:INVALID_DATASET)")); break;
  case  MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED:  Serial.printf(F("(RSP:SPECIFICATION_BY_GROUP_UNSUPPORTED)")); break;
  case  MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED:  Serial.printf(F("(RSP:SPECIFICATION_BY_DEPTH_UNSUPPORTED)")); break;
  case  MTP_RESPONSE_OBJECT_TOO_LARGE:  Serial.printf(F("(RSP:OBJECT_TOO_LARGE)")); break;
  case  MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED: Serial.printf(F("(RSP:OBJECT_PROP_NOT_SUPPORTED)")); break;
  case  MTP_EVENT_UNDEFINED: Serial.printf(F("(EVT:UNDEFINED)")); break;
  case  MTP_EVENT_CANCEL_TRANSACTION: Serial.printf(F("(EVT:CANCEL_TRANSACTION)")); break;
  case  MTP_EVENT_OBJECT_ADDED: Serial.printf(F("(EVT:OBJECT_ADDED)")); break;
  case  MTP_EVENT_OBJECT_REMOVED: Serial.printf(F("(EVT:OBJECT_REMOVED)")); break;
  case  MTP_EVENT_STORE_ADDED: Serial.printf(F("(EVT:STORE_ADDED)")); break;
  case  MTP_EVENT_STORE_REMOVED: Serial.printf(F("(EVT:STORE_REMOVED)")); break;
  case  MTP_EVENT_DEVICE_PROP_CHANGED: Serial.printf(F("(EVT:DEVICE_PROP_CHANGED)")); break;
  case  MTP_EVENT_OBJECT_INFO_CHANGED: Serial.printf(F("(EVT:OBJECT_INFO_CHANGED)")); break;
  case  MTP_EVENT_DEVICE_INFO_CHANGED: Serial.printf(F("(EVT:DEVICE_INFO_CHANGED)")); break;
  case  MTP_EVENT_REQUEST_OBJECT_TRANSFER: Serial.printf(F("(EVT:REQUEST_OBJECT_TRANSFER)")); break;
  case  MTP_EVENT_STORE_FULL: Serial.printf(F("(EVT:STORE_FULL)")); break;
  case  MTP_EVENT_DEVICE_RESET: Serial.printf(F("(EVT:DEVICE_RESET)")); break;
  case  MTP_EVENT_STORAGE_INFO_CHANGED: Serial.printf(F("(EVT:STORAGE_INFO_CHANGED)")); break;
  case  MTP_EVENT_CAPTURE_COMPLETE: Serial.printf(F("(EVT:CAPTURE_COMPLETE)")); break;
  case  MTP_EVENT_UNREPORTED_STATUS: Serial.printf(F("(EVT:UNREPORTED_STATUS)")); break;
  case  MTP_EVENT_OBJECT_PROP_CHANGED: Serial.printf(F("(EVT:OBJECT_PROP_CHANGED)")); break;
  case  MTP_EVENT_OBJECT_PROP_DESC_CHANGED: Serial.printf(F("(EVT:OBJECT_PROP_DESC_CHANGED)")); break;
  case  MTP_EVENT_OBJECT_REFERENCES_CHANGED: Serial.printf(F("(EVT:OBJECT_REFERENCES_CHANGED)")); break;
  }
  print("l:", c->len);

  Serial.printf(F(" T:%x"), c->transaction_id);
  if (c->len >= 16) Serial.printf(F(" P:%x"), c->params[0]);
  if (c->len >= 20) Serial.printf(F(" %x"), c->params[1]);
  if (c->len >= 24) Serial.printf(F(" %x"), c->params[2]);
  if (c->len >= 28) Serial.printf(F(" %x"), c->params[3]);
  if (c->len >= 32) Serial.printf(F(" %x"), c->params[4]);
  if (print_property_name >= 0) {
    switch (c->params[print_property_name]) {
      case MTP_PROPERTY_STORAGE_ID:  Serial.printf(" (STORAGE_ID)"); break;
      case MTP_PROPERTY_OBJECT_FORMAT:  Serial.printf(" (FORMAT)"); break;
      case MTP_PROPERTY_PROTECTION_STATUS:  Serial.printf(" (PROTECTION)"); break;
      case MTP_PROPERTY_OBJECT_SIZE:  Serial.printf(" (SIZE)"); break;
      case MTP_PROPERTY_OBJECT_FILE_NAME:  Serial.printf(" (OBJECT NAME)"); break;
      case MTP_PROPERTY_DATE_CREATED:  Serial.printf(" (CREATED)"); break;
      case MTP_PROPERTY_DATE_MODIFIED:  Serial.printf(" (MODIFIED)"); break;
      case MTP_PROPERTY_PARENT_OBJECT:  Serial.printf(" (PARENT)"); break;
      case MTP_PROPERTY_PERSISTENT_UID:  Serial.printf(" (PERSISTENT_UID)"); break;
      case MTP_PROPERTY_NAME:  Serial.printf(" (NAME)"); break;
    }
  }
  Serial.printf("\n");
}

/************************************************************/
//  Interrupt-based Data Movement
/************************************************************/

void MTPDevice::rx_callback(const Transfer_t *transfer)
{
  if (!transfer->driver) return;
  ((MTPDevice *)(transfer->driver))->rx_data(transfer);
}

void MTPDevice::tx_callback(const Transfer_t *transfer)
{
  if (!transfer->driver) return;
  ((MTPDevice *)(transfer->driver))->tx_data(transfer);
}

void MTPDevice::event_callback(const Transfer_t *transfer)
{
  if (!transfer->driver) return;
  ((MTPDevice *)(transfer->driver))->event_data(transfer);
}

void MTPDevice::rx_data(const Transfer_t *transfer)
{
  uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
  const uint8_t *p = (const uint8_t *)transfer->buffer;


  if (len > 0) {
    MTPContainer *c = (MTPContainer *)p;
    //print("rx token: ", transfer->qtd.token, HEX);
    //if (p == rx1) print("(RX1)");
    //if (p == rx2) print("(RX2)");
#if defined(DEBUG_MTP_VERBOSE)
    printContainer(c, "C<- ");
    print("    ");
    print_hexbytes(p, len);
#endif
    // Our processing depends on what type of message we received
    switch (c->type) {
    case MTP_CONTAINER_TYPE_COMMAND:
      processMTPCommand(c);
      break;
    case MTP_CONTAINER_TYPE_DATA:
      processMTPData(c);
      break;
    case MTP_CONTAINER_TYPE_RESPONSE:
      processMTPResponse(c);
      break;
    default:
      //case MTP_CONTAINER_TYPE_UNDEFINED:
      //case MTP_CONTAINER_TYPE_EVENT:
      break;
    }

  }
  queue_Data_Transfer(rxpipe_, (p == rx1) ? rx1 : rx2, rx_size_, this);
}

//=============================================================================
bool MTPDevice::process_object_added_event(uint32_t event_index) {

  prop_node_ = (storage_list_t *)extmem_malloc(sizeof(storage_list_t));

  if (!prop_node_) {
    DBGPrintf("Failed to allocate new item for added event\n");
    return true ; //
  }

  pending_events_[event_index].item_node = prop_node_;

  DBGPrintf("  %x - node allocated(%x)\n", pending_events_[event_index].id, (uint32_t)prop_node_);
  memset(prop_node_, 0, sizeof(storage_list_t));  // make sure every thing is zeroed out

  prop_node_->id =  pending_events_[event_index].id;
  prop_index_ = 0;
  pending_events_active_ = true; // we know we are processing an event...

  transaction_id_++;
  sendMsg(MTP_OPERATION_GET_OBJECT_PROP_VALUE, prop_node_->id, object_property_ids_[0]);
  return false;
}

bool MTPDevice::process_object_removed_event(uint32_t event_index) {
  storage_list_t *node = (storage_list_t *)findStorageItemByID(pending_events_[event_index].id);
  pending_events_[event_index].item_node = node;
  pending_events_[event_index].delete_node = true;
  DBGPrintf("process_object_removed_event: %x %x\n", pending_events_[event_index].id, (uint32_t)node);
  if (node) {
    // see if we can find item in parent list.
    storage_list_t *pnode = node->parent;
    if (pnode->child == node) pnode->child = node->next;  // unlinked from parent
    else {
      pnode = pnode->child;
      while (pnode && pnode->next != node) pnode = pnode->next;
      if (pnode) pnode->next = node->next;  // unlink us
    }
    node->next = nullptr; // don't want to free others in list later.
  }
  return true;
}


void MTPDevice::start_process_next_event() {
  bool event_completed = true;
  while (event_completed) {
    if (pending_events_head_ == pending_events_tail_) return;

    // there is something in the list...
    switch (pending_events_[pending_events_tail_].event) {
    case  MTP_EVENT_OBJECT_ADDED:
      event_completed = process_object_added_event(pending_events_tail_);
      break;
    case  MTP_EVENT_OBJECT_REMOVED:
      event_completed = process_object_removed_event(pending_events_tail_);
      break;
    case  MTP_EVENT_STORE_FULL:
    case  MTP_EVENT_STORAGE_INFO_CHANGED:
    case  MTP_EVENT_OBJECT_PROP_CHANGED:
      break; // will add in stuff soon
    }
    if (event_completed) {
      pending_events_active_ = true;
      complete_processing_event(false);
    }
  }
}

void MTPDevice::complete_processing_event(bool start_next_event) {
  if (pending_events_active_) {
    pending_events_active_ = false;
    if (peventCompeteCB_) (*peventCompeteCB_)((const event_data_t *)&pending_events_[pending_events_tail_]);

    // See if this item says to delete node.
    if (pending_events_[pending_events_tail_].delete_node && pending_events_[pending_events_tail_].item_node) {
      freeStorageListTree(pending_events_[pending_events_tail_].item_node);
    }

    pending_events_tail_++;
    if (pending_events_tail_ == MAX_PENDING_EVENTS) pending_events_tail_ = 0;
    if (start_next_event) start_process_next_event(); // maybe start process of next event if any
  }

}

void MTPDevice::event_data(const Transfer_t *transfer)
{
  uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);

  if (len > 0) {
    const uint8_t *p = (const uint8_t *)transfer->buffer;
    MTPContainer *c = (MTPContainer *)p;
    printContainer(c, "EVENT C<- ");
#if defined(DEBUG_MTP_VERBOSE)

    print("event token: ", transfer->qtd.token, HEX);
    print(" transfer length: ", transfer->length, DEC);
    print(" len:", len, DEC);
    print(" - ", *p, HEX);
    println(" ", *(p + 1), HEX);
    print("rx: ");
    print_hexbytes(p, len);
#endif


    // We will only process some of these.
    uint32_t id = 0;
    uint32_t prop_code = 0;
    switch (c->op) {
    case  MTP_EVENT_OBJECT_ADDED: id = c->params[0]; break;
    case  MTP_EVENT_OBJECT_REMOVED: id = c->params[0]; break;
    case  MTP_EVENT_STORE_FULL: id = c->params[0]; break;
    case  MTP_EVENT_STORAGE_INFO_CHANGED: id = c->params[0]; break;
    case  MTP_EVENT_OBJECT_PROP_CHANGED: id = c->params[0]; prop_code = c->params[1]; break;
    }
    // Lets see if we have room in our pending event list.
    if (id) {
      add_event_to_list(c->op, id, prop_code);
    }
  }
  queue_Data_Transfer(eventpipe_, rxevent, event_size_, this);
}


void MTPDevice::add_event_to_list(uint16_t op, uint32_t id, uint32_t prop_code=0)
{
  uint32_t head = pending_events_head_;
  uint32_t next_pending_event_head = head + 1;
  if (next_pending_event_head == MAX_PENDING_EVENTS) next_pending_event_head = 0;

  if (next_pending_event_head != pending_events_tail_) {
    // we have room.
    pending_events_[head].event = op;
    pending_events_[head].id = id;
    pending_events_[head].prop_code = prop_code;
    pending_events_[head].item_node = nullptr;
    pending_events_[head].delete_node = false;
    pending_events_head_ = next_pending_event_head;
    if (head == pending_events_tail_) start_process_next_event();

  } else {
    DBGPrintf(">>>>> Unprocessed Event queue is full");
  }

}

//=============================================================================

void MTPDevice::tx_data(const Transfer_t *transfer)
{
  uint8_t *p = (uint8_t *)transfer->buffer;

#if defined(DEBUG_MTP_VERBOSE)
  uint32_t len = transfer->length - ((transfer->qtd.token >> 16) & 0x7FFF);
  println("tx_data - length: ", len);
  if (len) print_hexbytes(p, len);
#endif

  // if we are in the process of tranfer, we may need to 
  // now read in the first N bytes of the file
 
  if (send_file_buffer_ptr_ && send_file_count_left_) {
    uint16_t cb_read = min (send_file_count_left_, tx_size_);
    Serial.printf("T");

    memcpy(p, send_file_buffer_ptr_, cb_read);
    send_file_buffer_ptr_ += cb_read; // setup pointer to first chunk o file
    send_file_count_left_ -= cb_read;  // probably should check how many were actually read...
    if (send_file_count_left_ == 0) {
      extmem_free(send_file_buffer_);
      send_file_buffer_ = nullptr;
      send_file_buffer_ptr_ = nullptr;
      Serial.printf("\n");
    }
    queue_Data_Transfer(txpipe_, p, cb_read, this);
  }
  
}


uint8_t MTPDevice::read8(const uint8_t **pdata) {
  const uint8_t *p = *pdata;
  uint8_t retval = *p++;
  *pdata = p;
  return retval;
}
uint16_t MTPDevice::read16(const uint8_t **pdata) {
  uint16_t *p = (uint16_t*)(*pdata);
  uint16_t retval = *p++;
  *pdata = (uint8_t*)p;
  return retval;
}

uint32_t MTPDevice::read32(const uint8_t **pdata) {
  uint32_t *p = (uint32_t*)(*pdata);
  uint32_t retval = *p++;
  *pdata = (uint8_t*)p;
  return retval;
}


uint64_t MTPDevice::read64(const uint8_t **pdata) {
  // crash if not aligned!
#if 1
  uint8_t *p = (uint8_t*)(*pdata);
  union {
    uint64_t u;
    uint8_t  b[8];
  } u64v;

  u64v.u = 0;

  for (uint8_t i = 0; i < 8; i++)u64v.b[i] = *p++;
  *pdata = (uint8_t*)p;
  return u64v.u;

#else
  uint64_t *p = (uint64_t*)(*pdata);
  Serial.printf(">>>>>>>Read64 %x\n", (uint32_t)p); Serial.flush();
  uint64_t retval = *p++;
  *pdata = (uint8_t*)p;
  return retval;
#endif
}

void MTPDevice::readStr(uint8_t *str, const uint8_t **pdata) {
  const uint8_t *p = *pdata;
  uint8_t str_len = *p++;
  while (str_len--) {
    *str++ = *p;
    p += 2;
  }
  *pdata = p;
  *str = 0; // null terminate the string.
}

uint8_t *MTPDevice::readAndAllocStr(const uint8_t **pdata)
{
  const uint8_t *p = *pdata;
  uint8_t str_len = *p++;

  uint8_t *pstr = (uint8_t*)extmem_malloc(str_len + 1);
  if (!pstr) return nullptr;

  uint8_t *palloc = pstr;

  while (str_len--) {
    *pstr++ = *p;
    p += 2;
  }
  *pdata = p;
  *pstr = 0; // null terminate the string.
  return palloc;
}

void MTPDevice::write8(uint8_t val, uint8_t **pdata) {
  uint8_t *p = *pdata;
  *pdata = p;
}
void MTPDevice::write16(uint16_t val, uint8_t **pdata) {
  uint16_t *p = (uint16_t*)(*pdata);
  *p++ = val;
  *pdata = (uint8_t*)p;
}

void MTPDevice::write32(uint32_t val, uint8_t **pdata) {
  uint32_t *p = (uint32_t*)(*pdata);
  *p++ = val;
  *pdata = (uint8_t*)p;
}


void MTPDevice::write64(uint64_t val, uint8_t **pdata) {
  // crash if not aligned!
  uint8_t *p = *pdata;
  union {
    uint64_t u;
    uint8_t  b[8];
  } u64v;

  u64v.u = val;

  for (uint8_t i = 0; i < 8; i++) *p++ = u64v.b[i];
  *pdata = p;
}

void MTPDevice::writeStr(const char *str, uint8_t **pdata) {
  uint8_t *p = *pdata;
  uint8_t str_len = strlen(str);
  if (str_len) {
    *p++ = ++str_len;
    while (str_len--) { *p++ = *str++; *p++ = 0;}
  } else *p++ = 0; // no string
  *pdata = p;
}



void MTPDevice::MTPDevice::processDescriptorData(MTPContainer *c) {
//C<- len:203 DATA: OP:1001(GET_DEVICE_INFO) TID:0 P:60064 640000 69006D14 72006300 73006F00
//  CB 00 00 00 02 00 01 10 00 00 00 00
//  64 00 06 00 00 00 64 00
//  14 6D 00 69 00 63 00 72 00 6F 00 73 00 6F 00 66 00 74 00 2E 00 63 00 6F 00 6D 00 3A 00 20 00 31 00 2E 00 30 00 3B 00 00 00
//  00 00
//  14 00 00 00 - 01 10 02 10 03 10 04 10 05 10 07 10 08 10 09 10 0B 10 0C 10 0D 10 14 10 15 10 19 10 1A 10 1B 10 01 98 02 98 03 98 04 98
//  00 00 00 00
//  01 00 00 00 02 D4 00 00 00 00 02 00 00 00 00 30 01 30 05 50 00 4A 00 52 00 43 00 00 00 07 54 00 65 00 65 00 6E 00 73 00 79 00 00 00 0F 31 00 2E 00 35 00 34 00 20 00 2F 00 20 00 4D 00 54 00 50 00 20 00 31 00 2E 00 30 00 00 00 08 38 00 35 00 39 00 37 00 34 00 34 00 30 00 00 00
  uint8_t str[256];
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);


  println("    STD Version:", read16(&pdata), HEX);
  println("    Vendor extesion ID:", read32(&pdata), HEX);
  println("    MTP Version:", read16(&pdata), HEX);
  Serial.flush();
  readStr(str, &pdata);
  Serial.flush();
  print("    Extensions:"); println((const char*)str);
  Serial.flush();
  println("    Mode:", read16(&pdata));
  // may for now ignore the operations, events and properties data to print
  uint32_t count = read32(&pdata);
  print(  "    Operations:");  while (count--) {print(" ", read16(&pdata), HEX);} println();
  count = read32(&pdata);
  print(  "    Events:");  while (count--) {print(" ", read16(&pdata), HEX);} println();
  count = read32(&pdata);
  print(  "    Device Properties:");  while (count--) {print(" ", read16(&pdata), HEX);} println();

  count = read32(&pdata);
  print(  "    Capture formats:");  while (count--) {print(" ", read16(&pdata), HEX);} println();
  count = read32(&pdata);

  // lets remember the formats
  print(  "    Playback formats:");
  cnt_object_formats_ = 0;
  while (count--) {
    uint16_t val = read16(&pdata);
    if (cnt_object_formats_ < (MAX_OBJECT_FORMATS - 1)) object_formats_[cnt_object_formats_++] = val;
    print(" ", val, HEX);
  } println();
  readStr(str, &pdata);
  print("    Manufacturer:"); println((const char *) str);
  readStr(str, &pdata);
  print("    Model:"); println((const char *) str);
  readStr(str, &pdata);
  print("    Serial:"); println((const char *) str);
}

void MTPDevice::processDevicePropDesc(MTPContainer *c)
{
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);
  // 30 00 00 00 02 00 14 10 01 00 00 00
  // 02 D4
  // FF FF
  // 00 07 54 00 65 00 65 00 6E 00 73 00 79 00 00 00 07 54 00 65 00 65 00 6E 00 73 00 79 00 00 00 00

  uint16_t device_property_code = read16(&pdata);
  uint16_t device_property_type = read16(&pdata);
  uint8_t read_write = read8(&pdata);
  switch (device_property_code) {
  case MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME:
    if (device_friendly_name_) extmem_free(device_friendly_name_);
    device_friendly_name_ =  readAndAllocStr(&pdata);
    DBGPrintf("DEVICE_FRIENDLY_NAME: %s\n", device_friendly_name_);
    break;
  default:
    DBGPrintf("processDevicePropDesc - Unexpected property code:%x type:%x RW:%x\n",
              device_property_code, device_property_type, read_write);
    break;
  }


}

void  MTPDevice::processGetStorageIDs(MTPContainer *c)
{
  // 2C 00 00 00 02 00 04 10 04 00 00 00
  // 07 00 00 00 01 00 00 00 02 00 00 00 03 00 00 00 04 00 00 00 05 00 00 00 06 00 00 00 07 00 00 00
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);
  print("Get Storage IDS:");
  cnt_storages_ =  read32(&pdata);
  if (cnt_storages_ > MAX_STORAGES) cnt_storages_ = MAX_STORAGES;
  for (uint8_t i = 0; i < cnt_storages_; i++) {
    storage_info_[i].storage.id = read32(&pdata);
    storage_info_[i].storage.storage_id = storage_info_[i].storage.id; // remember our index number...
    storage_info_[i].storage.format = 0x3001; // hack to say I think it is a directory like object.
    print(" ", storage_info_[i].storage.id, HEX );
  }
  // We have list of storages.
  println();

}

void MTPDevice::processGetStoreInfo(MTPContainer *c)
{
  // 32 00 00 00:02 00:05 10:08 00 00 00
  // 04 00 02 00 00 00 00 00 00 01 00 00 00 00 00 00 E5 00 00 00 00 00 FF FF FF FF 05 51 00 53 00 50 00 49 00 00 00 00
  // GetStore Info id:4 ST:0 FT:0 AC:100 Cap:0 Free:0 FreeO:53005105 NM: VOL:
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);

  uint8_t index = get_store_info_index_;
  storage_info_[index].storage_type = read16(&pdata);
  storage_info_[index].filesystem_type = read16(&pdata);
  storage_info_[index].access = read16(&pdata);
  storage_info_[index].max_capacity = read64(&pdata);
  storage_info_[index].free_space = read64(&pdata);
  storage_info_[index].free_space_objects = read32(&pdata);
  storage_info_[index].storage.parent = nullptr;
  storage_info_[index].storage.child = nullptr;

  if (storage_info_[index].storage.name) extmem_free(storage_info_[index].storage.name);
  if (storage_info_[index].volume_id) extmem_free(storage_info_[index].volume_id);

  storage_info_[index].storage.name = readAndAllocStr(&pdata);
  storage_info_[index].volume_id = readAndAllocStr(&pdata);
  DBGPrintf("GetStore Info id: %x ST:%x FT:%x AC:%x Cap:%llu Free:%llu FreeO:%u, NM:%s, VOL:%s\n",
            storage_info_[index].storage.id, storage_info_[index].storage_type, storage_info_[index].filesystem_type,
            storage_info_[index].access, storage_info_[index].max_capacity, storage_info_[index].free_space,
            storage_info_[index].free_space_objects, storage_info_[index].storage.name,
            storage_info_[index].volume_id);
}

void  MTPDevice::processObjectPropsSupported(MTPContainer *c)
{
  // 20 00 00 00:02 00:01 98:0A 00 00 00
  // 08 00 00 00  01 DC 02 DC 03 DC 04 DC 07 DC 0B DC 41 DC 44 DC
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);

  uint32_t cnt_props = read32(&pdata);
  DBGPrintf("processObjectPropsSupported: cnt:%u Add:", cnt_props);
  while (cnt_props--) {
    uint16_t prop_id = read16(&pdata);

    // see if it is in our list already
    uint8_t i;
    for (i = 0; i < cnt_object_property_ids_; i++) {
      if (prop_id == object_property_ids_[i]) break;
    }
    if (i == cnt_object_property_ids_) {
      object_property_ids_[cnt_object_property_ids_++] = prop_id;
      DBGPrintf(" %04x", prop_id);
    }
  }
  DBGPrintf("\n");
}

void MTPDevice::processObjectPropDesc(MTPContainer *c)
{
  //C-> len:16 CMD:  OP:9802(GET_OBJECT_PROP_DESC) TID:C P:DC01
  // 1A 00 00 00:02 00:02 98:0C 00 00 00
  // 01 DC 06 00 00 00 00 00 00 00 00 00 00 00

  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);
  uint16_t prop_id = read16(&pdata);
  uint16_t datatype = read16(&pdata);
  uint16_t getset = read8(&pdata);
  DBGPrintf("processObjectPropDesc(%04x) type:%x getset:%x\n", prop_id, datatype, getset);
}

void MTPDevice::processGetObjectHandles(MTPContainer *c)
{
  // 3C 00 00 00 02 00 07 10 13 00 00 00
  // 0B 00 00 00 11 00 00 00 10 00 00 00 0F 00 00 00 0E 00 00 00 0D 00 00 00 0C 00 00 00 0B 00 00 00 0A 00 00 00 09 00 00 00 08 00 00 00 07 00 00 00
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);
  uint32_t count = read32(&pdata);

  // Lets loop through maybe scavage the old child list of data.
  if (!enum_node_) {
    DBGPrintf("processGetObjectHandles called and we have no enum_node\n");
    return;
  }
  storage_list_t *old_child_list = enum_node_->child;
  enum_node_->child = nullptr;
  storage_list_t *last_added_child = nullptr;

  storage_list_t *prev_child = nullptr;
  storage_list_t *child = nullptr;
  while (count--) {
    uint32_t child_id = read32(&pdata);

    // See if this one is in our list or not.
    child = old_child_list;
    if (old_child_list) {
      prev_child = nullptr;
      while (child && child->id != child_id) {
        prev_child = child;
        child = child->next;
      }
    }

    if (child)  {
      // Found a match unlink from old list
      DBGPrintf("  %x - node reused\n", child_id);
      if (prev_child) prev_child->next = child->next;
      else old_child_list = child->next;
    } else {
      // need to create a new one
      child = (storage_list_t *)extmem_malloc(sizeof(storage_list_t));
      if (!child) break; //
      memset(child, 0, sizeof(storage_list_t));  // make sure every thing is zeroed out
      DBGPrintf("  %x - node allocated(%x)\n", child_id, (uint32_t)child);
      child->id = child_id; // put in the ID;
      child->parent = (storage_list_t *)enum_node_;
      child->name = nullptr; // we don't have it yet.
      child->size = 0;  /// clear it out.
      child->storage_id = enum_node_->storage_id; // again remember our storage index.
    }
    // add to end of current list.
    child->next = nullptr;
    if (last_added_child) last_added_child->next = child;
    else enum_node_->child = child; // we are the first one.
    last_added_child = child;   // remember for the next item
  }
  // Free up any old items that were not reused...
  // May need to recurse if these have children?
  freeStorageListTree(old_child_list);
}

const char *propertyNameFromID(uint32_t id) {
  typedef struct {
    uint32_t id;
    const char *sz;
  } id_to_sz_t;

  static const id_to_sz_t property_ids_to_name[] PROGMEM  = {
    {MTP_PROPERTY_STORAGE_ID, "STORAGE_ID"},
    {MTP_PROPERTY_OBJECT_FORMAT, "OBJECT_FORMAT"},
    {MTP_PROPERTY_PROTECTION_STATUS, "PROTECTION_STATUS"},
    {MTP_PROPERTY_OBJECT_SIZE, "OBJECT_SIZE"},
    {MTP_PROPERTY_OBJECT_FILE_NAME, "OBJECT_FILE_NAME"},
    {MTP_PROPERTY_DATE_CREATED, "DATE_CREATED"},
    {MTP_PROPERTY_DATE_MODIFIED, "DATE_MODIFIED"},
    {MTP_PROPERTY_PARENT_OBJECT, "PARENT_OBJECT"},
    {MTP_PROPERTY_PERSISTENT_UID, "PERSISTENT_UID"},
    {MTP_PROPERTY_NAME, "NAME"}
  };
  for (uint8_t i = 0; i < (sizeof(property_ids_to_name) / sizeof(property_ids_to_name[0])); i++) {
    if (property_ids_to_name[i].id == id) return property_ids_to_name[i].sz;
  }
  static const char empty_str[] = "";
  return empty_str;

}

void MTPDevice::processGetObjectPropValue(MTPContainer *c) {
  // 17 00 00 00 02 00 03 98 13 00 00 00
  // 05 44 00 69 00 72 00 30 00 00 00
  const uint8_t * pdata = (uint8_t*)c + sizeof(MTPHeader);

  DBGPrintf("processGetObjectPropValue(%s): %x %x %x: %x %x %x %x\n", propertyNameFromID(object_property_ids_[prop_index_]),
            (uint32_t)prop_node_, prop_index_,
            object_property_ids_[prop_index_], pdata[0], pdata[1], pdata[2], pdata[3]);
  // I don't think the data has anything in it what was asked for...
  if (!prop_node_) return;
  // For now only supporting a few of them
  switch (object_property_ids_[prop_index_]) {
  case MTP_PROPERTY_STORAGE_ID:         //0xDC01:
  {
    uint32_t storage_id = read32(&pdata);
    if (prop_node_->storage_id == 0) prop_node_->storage_id = storage_id;
    else if (prop_node_->storage_id != storage_id) DBGPrintf("    storage_ids changed? %x != %x",
          prop_node_->storage_id, storage_id);
    prop_node_->storage_id = storage_id;
  }
  break;
  case MTP_PROPERTY_OBJECT_FORMAT:      //0xDC02:
    prop_node_->format = read16(&pdata);           //(dir?0x3001:0x3000);
    break;
  case MTP_PROPERTY_PROTECTION_STATUS:  //0xDC03:
    //write16(0);
    break;
  case MTP_PROPERTY_OBJECT_SIZE:        //0xDC04:
    //14 00 00 00 02 00 03 98 17 00 00 00 FF FF FF FF 00 00 00 00
    prop_node_->size = read64(&pdata);
    break;
  case MTP_PROPERTY_OBJECT_FILE_NAME:   //0xDC07:
    // not sure difference with NAME below...
    if ( prop_node_->name ) extmem_free(prop_node_->name );
    prop_node_->name =   readAndAllocStr(&pdata);
    break;
  case MTP_PROPERTY_DATE_CREATED:       //0xDC08:
    //writestring("");
    break;
  case MTP_PROPERTY_DATE_MODIFIED:      //0xDC09:
    readStr(prop_node_->modify_date, &pdata); // save away the device date... 

    //writestring("");
    break;
  case MTP_PROPERTY_PARENT_OBJECT:      //0xDC0B:
  {
    uint32_t parent_id = read32(&pdata);
    if (prop_node_->parent) {
      if (prop_node_->parent->id != parent_id) DBGPrintf("    Parent ID changed? %x != %x",
            prop_node_->parent->id, parent_id);
    } else {
      // We need to find the parent object...
      storage_list_t *parent_item = nullptr;
      if (prop_node_->storage_id) {
        for (uint8_t i = 0; i < cnt_storages_; i++) {
          if (storage_info_[i].storage.id == prop_node_->storage_id) {
            parent_item = (storage_list_t *)findStorageItem(storage_info_[i].storage.child, parent_id);
            break;
          }
        }
      } else parent_item = (storage_list_t *)findStorageItemByID(parent_id);

      // link us in if we found the parent, add at end of it's list so that we
      // wont reiterate reading the properties of the other items...
      DBGPrintf("    Parent set: %x\n", (uint32_t)parent_item);
      if (parent_item) {
        prop_node_->parent = parent_item;                   // set our parent pointer
        prop_node_->storage_id = parent_item->storage_id;   // and storage id again...
        prop_node_->next = nullptr;                         // Should already been set but for end of list

        if (parent_item->child) {
          parent_item = parent_item->child;                 // reuse variable.
          while (parent_item->next) parent_item = parent_item->next;  // find end of list
          parent_item->next = prop_node_;                   // linked in as new end
        } else {
          parent_item->child = prop_node_;                  // We are the first item...
        }
      }
    }
  }
  break;
  case MTP_PROPERTY_PERSISTENT_UID:     //0xDC41:
    //write32(p1);
    //write32(parent);
    //write32(storage);
    //write32(0);
    break;
  case MTP_PROPERTY_NAME:               //0xDC44:
    if ( prop_node_->name ) extmem_free(prop_node_->name );
    prop_node_->name =   readAndAllocStr(&pdata);
    break;
  default:
    break;
  }

}

//=============================================================================
void MTPDevice::processMTPCommand(MTPContainer * c) {

}


//=============================================================================
void MTPDevice::processMTPData(MTPContainer * c) {

  VDBGPrintf("::processMTPData\n");
  switch (c->op) {
  case MTP_OPERATION_GET_DEVICE_INFO:
    processDescriptorData(c);
    break;
  case MTP_OPERATION_GET_DEVICE_PROP_DESC:
    processDevicePropDesc(c);
    break;
  case MTP_OPERATION_GET_STORAGE_IDS:
    processGetStorageIDs(c);
    break;
  case MTP_OPERATION_GET_STORAGE_INFO:
    processGetStoreInfo(c);
    break;
  case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
    processObjectPropsSupported(c);
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_DESC:
    processObjectPropDesc(c);
    break;
  case MTP_OPERATION_GET_OBJECT_HANDLES:
    processGetObjectHandles(c);
    break;
  case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
    processGetObjectPropValue(c);
    break;
  default:
    break;
  }


}

//=============================================================================
void MTPDevice::processMTPResponse(MTPContainer * c) {

  // see if the command succeeded...
  last_response_ = c->op; // remember the last response.
  if (c->op == MTP_RESPONSE_OK) {

    // probably will be some state table
    switch (last_mtp_op_) {
    case MTP_OPERATION_GET_DEVICE_INFO:
      // lets open up a session
      session_id_ = 42; // answer to everything ;)
      sendMsg(MTP_OPERATION_OPEN_SESSION, session_id_);

      break;
    case MTP_OPERATION_OPEN_SESSION:
      transaction_id_++;
      sendMsg(MTP_OPERATION_GET_DEVICE_PROP_DESC, MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME);
      break;
    case MTP_OPERATION_GET_DEVICE_PROP_DESC:
      transaction_id_++;
      sendMsg(MTP_OPERATION_GET_STORAGE_IDS);
      break;
    case MTP_OPERATION_GET_STORAGE_IDS:
      if (cnt_storages_) {
        transaction_id_++;
        get_store_info_index_ = 0;
        sendMsg(MTP_OPERATION_GET_STORAGE_INFO, storage_info_[0].storage.id);
      }
      break;
    case MTP_OPERATION_GET_STORAGE_INFO:
      if (++get_store_info_index_ < cnt_storages_) {
        transaction_id_++;
        sendMsg(MTP_OPERATION_GET_STORAGE_INFO, storage_info_[get_store_info_index_].storage.id);
      } else if (cnt_object_formats_) {
        transaction_id_++;
        get_store_info_index_ = 0;  // reuse for objects below
        sendMsg(MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED, object_formats_[0]);
      }
      break;
    case MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED:
      if (++get_store_info_index_ < cnt_object_formats_) {
        transaction_id_++;
        sendMsg(MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED, object_formats_[get_store_info_index_]);
      } else if (cnt_object_property_ids_) {
        transaction_id_++;
        get_store_info_index_ = 0;  // reuse for objects below
        sendMsg(MTP_OPERATION_GET_OBJECT_PROP_DESC, object_property_ids_[0]);
      }
      break;
    case MTP_OPERATION_GET_OBJECT_PROP_DESC:
      if (++get_store_info_index_ < cnt_object_property_ids_) {
        transaction_id_++;
        sendMsg(MTP_OPERATION_GET_OBJECT_PROP_DESC, object_property_ids_[get_store_info_index_]);
      } else   setup_complete_ = true;  // moving target
      break;

    case MTP_OPERATION_GET_OBJECT_HANDLES:
      if (enum_node_ && enum_node_->child) {
        prop_node_ = enum_node_->child;
        prop_index_ = 0;
        transaction_id_++;
        sendMsg(MTP_OPERATION_GET_OBJECT_PROP_VALUE, prop_node_->id, object_property_ids_[0]);
      } else enum_node_ = nullptr;
      break;
    case MTP_OPERATION_GET_OBJECT_PROP_VALUE:
      if (prop_node_) {
        if (++prop_index_ >= cnt_object_property_ids_)  {
          prop_node_ = prop_node_->next;
          prop_index_ = 0;
        }
      }
      transaction_id_++;
      if (prop_node_) sendMsg(MTP_OPERATION_GET_OBJECT_PROP_VALUE, prop_node_->id, object_property_ids_[prop_index_]);
      else {
        enum_node_ = nullptr;
        if (pending_events_active_) complete_processing_event(true);
      }
      break;
    case MTP_OPERATION_SEND_OBJECT_INFO:
      sendObjectMsg(c->params[0], c->params[1], c->params[2]);
      break;
        
    case MTP_OPERATION_SEND_OBJECT:
      // We completed transfer, lets ask for some details back
      Serial.printf("\nMTP_OPERATION_SEND_OBJECT  completed new Object: %x", send_file_object_id_);
      add_event_to_list(MTP_EVENT_OBJECT_ADDED, send_file_object_id_, 0);
      break;

    default:
      DBGPrintf("Last operation: %x completed OK\n", last_mtp_op_);
      printContainer(c, "R-> ");
      break;
    }

  } else {
    DBGPrintf("Last operation: %x return response:%x\n", last_mtp_op_, c->op);
    printContainer(c, "R-> ");
  }
}
