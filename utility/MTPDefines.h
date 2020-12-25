/* USB EHCI Host for Teensy 3.6 and Teensy 4.x
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

#ifndef USB_HOST_MTP_DEFINES_H_
#define USB_HOST_MTP_DEFINES_H_

/***************************************************************************************************/
// Container Types
#define MTP_CONTAINER_TYPE_UNDEFINED    0
#define MTP_CONTAINER_TYPE_COMMAND      1
#define MTP_CONTAINER_TYPE_DATA         2
#define MTP_CONTAINER_TYPE_RESPONSE     3
#define MTP_CONTAINER_TYPE_EVENT        4

// Container Offsets
#define MTP_CONTAINER_LENGTH_OFFSET             0
#define MTP_CONTAINER_TYPE_OFFSET               4
#define MTP_CONTAINER_CODE_OFFSET               6
#define MTP_CONTAINER_TRANSACTION_ID_OFFSET     8
#define MTP_CONTAINER_PARAMETER_OFFSET          12
#define MTP_CONTAINER_HEADER_SIZE               12

// MTP Operation Codes
#define MTP_OPERATION_GET_DEVICE_INFO                       0x1001
#define MTP_OPERATION_OPEN_SESSION                          0x1002
#define MTP_OPERATION_CLOSE_SESSION                         0x1003
#define MTP_OPERATION_GET_STORAGE_IDS                       0x1004
#define MTP_OPERATION_GET_STORAGE_INFO                      0x1005
#define MTP_OPERATION_GET_NUM_OBJECTS                       0x1006
#define MTP_OPERATION_GET_OBJECT_HANDLES                    0x1007
#define MTP_OPERATION_GET_OBJECT_INFO                       0x1008
#define MTP_OPERATION_GET_OBJECT                            0x1009
#define MTP_OPERATION_GET_THUMB                             0x100A
#define MTP_OPERATION_DELETE_OBJECT                         0x100B
#define MTP_OPERATION_SEND_OBJECT_INFO                      0x100C
#define MTP_OPERATION_SEND_OBJECT                           0x100D
#define MTP_OPERATION_INITIATE_CAPTURE                      0x100E
#define MTP_OPERATION_FORMAT_STORE                          0x100F
#define MTP_OPERATION_RESET_DEVICE                          0x1010
#define MTP_OPERATION_SELF_TEST                             0x1011
#define MTP_OPERATION_SET_OBJECT_PROTECTION                 0x1012
#define MTP_OPERATION_POWER_DOWN                            0x1013
#define MTP_OPERATION_GET_DEVICE_PROP_DESC                  0x1014
#define MTP_OPERATION_GET_DEVICE_PROP_VALUE                 0x1015
#define MTP_OPERATION_SET_DEVICE_PROP_VALUE                 0x1016
#define MTP_OPERATION_RESET_DEVICE_PROP_VALUE               0x1017
#define MTP_OPERATION_TERMINATE_OPEN_CAPTURE                0x1018
#define MTP_OPERATION_MOVE_OBJECT                           0x1019
#define MTP_OPERATION_COPY_OBJECT                           0x101A
#define MTP_OPERATION_GET_PARTIAL_OBJECT                    0x101B
#define MTP_OPERATION_INITIATE_OPEN_CAPTURE                 0x101C
#define MTP_OPERATION_GET_OBJECT_PROPS_SUPPORTED            0x9801
#define MTP_OPERATION_GET_OBJECT_PROP_DESC                  0x9802
#define MTP_OPERATION_GET_OBJECT_PROP_VALUE                 0x9803
#define MTP_OPERATION_SET_OBJECT_PROP_VALUE                 0x9804
#define MTP_OPERATION_GET_OBJECT_PROP_LIST                  0x9805
#define MTP_OPERATION_SET_OBJECT_PROP_LIST                  0x9806
#define MTP_OPERATION_GET_INTERDEPENDENT_PROP_DESC          0x9807
#define MTP_OPERATION_SEND_OBJECT_PROP_LIST                 0x9808
#define MTP_OPERATION_GET_OBJECT_REFERENCES                 0x9810
#define MTP_OPERATION_SET_OBJECT_REFERENCES                 0x9811
#define MTP_OPERATION_SKIP                                  0x9820

// MTP Object Property Codes

#define MTP_PROPERTY_STORAGE_ID                             0xDC01
#define MTP_PROPERTY_OBJECT_FORMAT                          0xDC02
#define MTP_PROPERTY_PROTECTION_STATUS                      0xDC03
#define MTP_PROPERTY_OBJECT_SIZE                            0xDC04
#define MTP_PROPERTY_OBJECT_FILE_NAME                       0xDC07
#define MTP_PROPERTY_DATE_CREATED                           0xDC08
#define MTP_PROPERTY_DATE_MODIFIED                          0xDC09
#define MTP_PROPERTY_PARENT_OBJECT                          0xDC0B
#define MTP_PROPERTY_PERSISTENT_UID                         0xDC41
#define MTP_PROPERTY_NAME                                   0xDC44

// MTP Device Property Codes

#define MTP_DEVICE_PROPERTY_UNDEFINED                       0x5000
#define MTP_DEVICE_PROPERTY_BATTERY_LEVEL                   0x5001
#define MTP_DEVICE_PROPERTY_FUNCTIONAL_MODE                 0x5002
#define MTP_DEVICE_PROPERTY_IMAGE_SIZE                      0x5003
#define MTP_DEVICE_PROPERTY_COMPRESSION_SETTING             0x5004
#define MTP_DEVICE_PROPERTY_WHITE_BALANCE                   0x5005
#define MTP_DEVICE_PROPERTY_RGB_GAIN                        0x5006
#define MTP_DEVICE_PROPERTY_F_NUMBER                        0x5007
#define MTP_DEVICE_PROPERTY_FOCAL_LENGTH                    0x5008
#define MTP_DEVICE_PROPERTY_FOCUS_DISTANCE                  0x5009
#define MTP_DEVICE_PROPERTY_FOCUS_MODE                      0x500A
#define MTP_DEVICE_PROPERTY_EXPOSURE_METERING_MODE          0x500B
#define MTP_DEVICE_PROPERTY_FLASH_MODE                      0x500C
#define MTP_DEVICE_PROPERTY_EXPOSURE_TIME                   0x500D
#define MTP_DEVICE_PROPERTY_EXPOSURE_PROGRAM_MODE           0x500E
#define MTP_DEVICE_PROPERTY_EXPOSURE_INDEX                  0x500F
#define MTP_DEVICE_PROPERTY_EXPOSURE_BIAS_COMPENSATION      0x5010
#define MTP_DEVICE_PROPERTY_DATETIME                        0x5011
#define MTP_DEVICE_PROPERTY_CAPTURE_DELAY                   0x5012
#define MTP_DEVICE_PROPERTY_STILL_CAPTURE_MODE              0x5013
#define MTP_DEVICE_PROPERTY_CONTRAST                        0x5014
#define MTP_DEVICE_PROPERTY_SHARPNESS                       0x5015
#define MTP_DEVICE_PROPERTY_DIGITAL_ZOOM                    0x5016
#define MTP_DEVICE_PROPERTY_EFFECT_MODE                     0x5017
#define MTP_DEVICE_PROPERTY_BURST_NUMBER                    0x5018
#define MTP_DEVICE_PROPERTY_BURST_INTERVAL                  0x5019
#define MTP_DEVICE_PROPERTY_TIMELAPSE_NUMBER                0x501A
#define MTP_DEVICE_PROPERTY_TIMELAPSE_INTERVAL              0x501B
#define MTP_DEVICE_PROPERTY_FOCUS_METERING_MODE             0x501C
#define MTP_DEVICE_PROPERTY_UPLOAD_URL                      0x501D
#define MTP_DEVICE_PROPERTY_ARTIST                          0x501E
#define MTP_DEVICE_PROPERTY_COPYRIGHT_INFO                  0x501F
#define MTP_DEVICE_PROPERTY_SYNCHRONIZATION_PARTNER         0xD401
#define MTP_DEVICE_PROPERTY_DEVICE_FRIENDLY_NAME            0xD402
#define MTP_DEVICE_PROPERTY_VOLUME                          0xD403
#define MTP_DEVICE_PROPERTY_SUPPORTED_FORMATS_ORDERED       0xD404
#define MTP_DEVICE_PROPERTY_DEVICE_ICON                     0xD405
#define MTP_DEVICE_PROPERTY_PLAYBACK_RATE                   0xD410
#define MTP_DEVICE_PROPERTY_PLAYBACK_OBJECT                 0xD411
#define MTP_DEVICE_PROPERTY_PLAYBACK_CONTAINER_INDEX        0xD412
#define MTP_DEVICE_PROPERTY_SESSION_INITIATOR_VERSION_INFO  0xD406
#define MTP_DEVICE_PROPERTY_PERCEIVED_DEVICE_TYPE           0xD407

// MTP Events

#define MTP_EVENT_UNDEFINED                         0x4000
#define MTP_EVENT_CANCEL_TRANSACTION                0x4001
#define MTP_EVENT_OBJECT_ADDED                      0x4002
#define MTP_EVENT_OBJECT_REMOVED                    0x4003
#define MTP_EVENT_STORE_ADDED                       0x4004
#define MTP_EVENT_STORE_REMOVED                     0x4005
#define MTP_EVENT_DEVICE_PROP_CHANGED               0x4006
#define MTP_EVENT_OBJECT_INFO_CHANGED               0x4007
#define MTP_EVENT_DEVICE_INFO_CHANGED               0x4008
#define MTP_EVENT_REQUEST_OBJECT_TRANSFER           0x4009
#define MTP_EVENT_STORE_FULL                        0x400A
#define MTP_EVENT_DEVICE_RESET                      0x400B
#define MTP_EVENT_STORAGE_INFO_CHANGED              0x400C
#define MTP_EVENT_CAPTURE_COMPLETE                  0x400D
#define MTP_EVENT_UNREPORTED_STATUS                 0x400E
#define MTP_EVENT_OBJECT_PROP_CHANGED               0xC801
#define MTP_EVENT_OBJECT_PROP_DESC_CHANGED          0xC802
#define MTP_EVENT_OBJECT_REFERENCES_CHANGED         0xC803

// Responses

#define MTP_RESPONSE_UNDEFINED                                  0x2000
#define MTP_RESPONSE_OK                                         0x2001
#define MTP_RESPONSE_GENERAL_ERROR                              0x2002
#define MTP_RESPONSE_SESSION_NOT_OPEN                           0x2003
#define MTP_RESPONSE_INVALID_TRANSACTION_ID                     0x2004
#define MTP_RESPONSE_OPERATION_NOT_SUPPORTED                    0x2005
#define MTP_RESPONSE_PARAMETER_NOT_SUPPORTED                    0x2006
#define MTP_RESPONSE_INCOMPLETE_TRANSFER                        0x2007
#define MTP_RESPONSE_INVALID_STORAGE_ID                         0x2008
#define MTP_RESPONSE_INVALID_OBJECT_HANDLE                      0x2009
#define MTP_RESPONSE_DEVICE_PROP_NOT_SUPPORTED                  0x200A
#define MTP_RESPONSE_INVALID_OBJECT_FORMAT_CODE                 0x200B
#define MTP_RESPONSE_STORAGE_FULL                               0x200C
#define MTP_RESPONSE_OBJECT_WRITE_PROTECTED                     0x200D
#define MTP_RESPONSE_STORE_READ_ONLY                            0x200E
#define MTP_RESPONSE_ACCESS_DENIED                              0x200F
#define MTP_RESPONSE_NO_THUMBNAIL_PRESENT                       0x2010
#define MTP_RESPONSE_SELF_TEST_FAILED                           0x2011
#define MTP_RESPONSE_PARTIAL_DELETION                           0x2012
#define MTP_RESPONSE_STORE_NOT_AVAILABLE                        0x2013
#define MTP_RESPONSE_SPECIFICATION_BY_FORMAT_UNSUPPORTED        0x2014
#define MTP_RESPONSE_NO_VALID_OBJECT_INFO                       0x2015
#define MTP_RESPONSE_INVALID_CODE_FORMAT                        0x2016
#define MTP_RESPONSE_UNKNOWN_VENDOR_CODE                        0x2017
#define MTP_RESPONSE_CAPTURE_ALREADY_TERMINATED                 0x2018
#define MTP_RESPONSE_DEVICE_BUSY                                0x2019
#define MTP_RESPONSE_INVALID_PARENT_OBJECT                      0x201A
#define MTP_RESPONSE_INVALID_DEVICE_PROP_FORMAT                 0x201B
#define MTP_RESPONSE_INVALID_DEVICE_PROP_VALUE                  0x201C
#define MTP_RESPONSE_INVALID_PARAMETER                          0x201D
#define MTP_RESPONSE_SESSION_ALREADY_OPEN                       0x201E
#define MTP_RESPONSE_TRANSACTION_CANCELLED                      0x201F
#define MTP_RESPONSE_SPECIFICATION_OF_DESTINATION_UNSUPPORTED   0x2020
#define MTP_RESPONSE_INVALID_OBJECT_PROP_CODE                   0xA801
#define MTP_RESPONSE_INVALID_OBJECT_PROP_FORMAT                 0xA802
#define MTP_RESPONSE_INVALID_OBJECT_PROP_VALUE                  0xA803
#define MTP_RESPONSE_INVALID_OBJECT_REFERENCE                   0xA804
#define MTP_RESPONSE_GROUP_NOT_SUPPORTED                        0xA805
#define MTP_RESPONSE_INVALID_DATASET                            0xA806
#define MTP_RESPONSE_SPECIFICATION_BY_GROUP_UNSUPPORTED         0xA807
#define MTP_RESPONSE_SPECIFICATION_BY_DEPTH_UNSUPPORTED         0xA808
#define MTP_RESPONSE_OBJECT_TOO_LARGE                           0xA809
#define MTP_RESPONSE_OBJECT_PROP_NOT_SUPPORTED                  0xA80A

#endif