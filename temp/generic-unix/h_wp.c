/**************************************************************************
*
* Copyright (C) 2005 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "config.h"
#include "txbuf.h"
#include "bacdef.h"
#include "bacdcode.h"
#include "bacerror.h"
#include "apdu.h"
#include "npdu.h"
#include "abort.h"
#include "wp.h"
/* device object has the handling for all objects */
#include "device.h"
#include "handlers.h"

#include "sedona.h"

/** @file h_wp.c  Handles Write Property requests. */


/** Handler for a WriteProperty Service request.
 * @ingroup DSWP
 * This handler will be invoked by apdu_handler() if it has been enabled
 * by a call to apdu_set_confirmed_handler().
 * This handler builds a response packet, which is
 * - an Abort if
 *   - the message is segmented
 *   - if decoding fails
 * - an ACK if Device_Write_Property() succeeds
 * - an Error if Device_Write_Property() fails
 *   or there isn't enough room in the APDU to fit the data.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */


//Titus
uint8_t Handler_Transmit_Buffer[MAX_PDU] = { 0 };

void handler_write_property(
    uint8_t * service_request,
    uint16_t service_len,
    BACNET_ADDRESS * src,
    BACNET_CONFIRMED_SERVICE_DATA * service_data)
{
    BACNET_WRITE_PROPERTY_DATA wp_data;
    int len = 0;
    int pdu_len = 0;
    BACNET_NPDU_DATA npdu_data;
    int bytes_sent = 0;
    BACNET_ADDRESS my_address;


//uint8_t *service_request;
//uint16_t service_len;
//BACNET_ADDRESS *src;
//BACNET_CONFIRMED_SERVICE_DATA * service_data ;

/*
typedef struct _confirmed_service_data {
    bool segmented_message;
    bool more_follows;
    bool segmented_response_accepted;
    int max_segs;
    int max_resp;
    uint8_t invoke_id;
    uint8_t sequence_number;
    uint8_t proposed_window_number;
} BACNET_CONFIRMED_SERVICE_DATA;
*/


//    printf("service_data->segmented_message %d service_data->more_follows %d service_data->segmented_response_accepted %d service_data->max_segs %d service_data->max_resp %d service_data->invoke_id %d service_data->sequence_number %d service_data->proposed_window_number %d \n",service_data->segmented_message, service_data->more_follows, service_data->segmented_response_accepted, service_data->max_segs, service_data->max_resp, service_data->invoke_id, service_data->sequence_number, service_data->proposed_window_number);


//    printf("src %p service_request %p service_len %d  service_data %p\n",src,service_request,service_len,service_data );


    /* encode the NPDU portion of the packet */
    datalink_get_my_address(&my_address);
    npdu_encode_npdu_data(&npdu_data, false, MESSAGE_PRIORITY_NORMAL);
    pdu_len =
        npdu_encode_pdu(&Handler_Transmit_Buffer[0], src, &my_address,
        &npdu_data);
#if PRINT_ENABLED
    fprintf(stderr, "WP: Received Request!\n");
#endif
    if (service_data->segmented_message) {
        len =
            abort_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data->invoke_id, ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
            true);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Segmented message.  Sending Abort!\n");
#endif
        goto WP_ABORT;
    }   /* decode the service request only */
    len = wp_decode_service_request(service_request, service_len, &wp_data);
#if PRINT_ENABLED
    if (len > 0)
        fprintf(stderr,
            "WP: type=%lu instance=%lu property=%lu priority=%lu index=%ld\n",
            (unsigned long) wp_data.object_type,
            (unsigned long) wp_data.object_instance,
            (unsigned long) wp_data.object_property,
            (unsigned long) wp_data.priority, (long) wp_data.array_index);
    else
        fprintf(stderr, "WP: Unable to decode Request!\n");
#endif
    /* bad decoding or something we didn't understand - send an abort */
    if (len <= 0) {
        len =
            abort_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data->invoke_id, ABORT_REASON_OTHER, true);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Bad Encoding. Sending Abort!\n");
#endif
        goto WP_ABORT;
    }
    if (Device_Write_Property(&wp_data)) {
        len =
            encode_simple_ack(&Handler_Transmit_Buffer[pdu_len],
            service_data->invoke_id, SERVICE_CONFIRMED_WRITE_PROPERTY);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Sending Simple Ack!\n");
#endif
    } else {
        len =
            bacerror_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data->invoke_id, SERVICE_CONFIRMED_WRITE_PROPERTY,
            wp_data.error_class, wp_data.error_code);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Sending Error!\n");
#endif
    }
  WP_ABORT:
    pdu_len += len;
    bytes_sent =
        datalink_send_pdu(src, &npdu_data, &Handler_Transmit_Buffer[0],
        pdu_len);
#if PRINT_ENABLED
    if (bytes_sent <= 0) {
        fprintf(stderr, "WP: Failed to send PDU (%s)!\n", strerror(errno));
    }
#else
    bytes_sent = bytes_sent;
#endif

    return;
}


//Not required
#if 0

//void handler_write_property(
//    uint8_t * service_request,
//    uint16_t service_len,
//    BACNET_ADDRESS * src,
//    BACNET_CONFIRMED_SERVICE_DATA * service_data)
Cell BACnet_BACnetDev_doTest(SedonaVM* vm, Cell* params)
{

//Titus
uint8_t *service_request;
uint16_t service_len = 13;
BACNET_ADDRESS *src;
BACNET_CONFIRMED_SERVICE_DATA service_data ;

//    printf("BACnet doTest called! PROBE1 %d \n",level2);
	return;

    BACNET_WRITE_PROPERTY_DATA wp_data;
    int len = 0;
    int pdu_len = 0;
    BACNET_NPDU_DATA npdu_data;
    int bytes_sent = 0;
    BACNET_ADDRESS my_address;

    printf("PROBE1.01!\n");

	wp_data.object_instance = 3;
	wp_data.object_type = 4;
	wp_data.object_property = 85; //PROP_PRESENT_VALUE=85
	wp_data.array_index = -1;
	wp_data.priority = 1;

    printf("PROBE1.02!\n");

service_data.segmented_message = 0;
service_data.more_follows = 0;
service_data.segmented_response_accepted = 0;
service_data.max_segs = 0;
service_data.max_resp = 1476;
service_data.invoke_id = 60;
service_data.sequence_number = 0;
service_data.proposed_window_number = 0;

//    printf("PROBE1.03!\n");

//    printf("service_data->segmented_message %d service_data->more_follows %d service_data->segmented_response_accepted %d service_data->max_segs %d service_data->max_resp %d service_data->invoke_id %d service_data->sequence_number %d service_data->proposed_window_number %d \n",service_data.segmented_message, service_data.more_follows, service_data.segmented_response_accepted, service_data.max_segs, service_data.max_resp, service_data.invoke_id, service_data.sequence_number, service_data.proposed_window_number);


    /* encode the NPDU portion of the packet */
    datalink_get_my_address(&my_address);
//    printf("PROBE1.1!\n");
    npdu_encode_npdu_data(&npdu_data, false, MESSAGE_PRIORITY_NORMAL);
    pdu_len =
        npdu_encode_pdu(&Handler_Transmit_Buffer[0], src, &my_address,
        &npdu_data);

//    printf("PROBE1.2!\n");

#if PRINT_ENABLED
    fprintf(stderr, "WP: Received Request!\n");
#endif
    if (service_data.segmented_message) {
        len =
            abort_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data.invoke_id, ABORT_REASON_SEGMENTATION_NOT_SUPPORTED,
            true);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Segmented message.  Sending Abort!\n");
#endif
        goto WP_ABORT;
    }   /* decode the service request only */

//    printf("PROBE2!\n");


    len = wp_decode_service_request(service_request, service_len, &wp_data);
//	len = 10;//hardcoded
#if PRINT_ENABLED
    if (len > 0)
        fprintf(stderr,
            "WP: type=%lu instance=%lu property=%lu priority=%lu index=%ld\n",
            (unsigned long) wp_data.object_type,
            (unsigned long) wp_data.object_instance,
            (unsigned long) wp_data.object_property,
            (unsigned long) wp_data.priority, (long) wp_data.array_index);
    else
        fprintf(stderr, "WP: Unable to decode Request!\n");
#endif
    /* bad decoding or something we didn't understand - send an abort */
    if (len <= 0) {
        len =
            abort_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data.invoke_id, ABORT_REASON_OTHER, true);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Bad Encoding. Sending Abort!\n");
#endif
        goto WP_ABORT;
    }
    if (Device_Write_Property(&wp_data)) {
        len =
            encode_simple_ack(&Handler_Transmit_Buffer[pdu_len],
            service_data.invoke_id, SERVICE_CONFIRMED_WRITE_PROPERTY);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Sending Simple Ack!\n");
#endif
    } else {
        len =
            bacerror_encode_apdu(&Handler_Transmit_Buffer[pdu_len],
            service_data.invoke_id, SERVICE_CONFIRMED_WRITE_PROPERTY,
            wp_data.error_class, wp_data.error_code);
#if PRINT_ENABLED
        fprintf(stderr, "WP: Sending Error!\n");
#endif
    }

  WP_ABORT:
    pdu_len += len;
//    printf("PROBE3!\n");
    bytes_sent =
        datalink_send_pdu(src, &npdu_data, &Handler_Transmit_Buffer[0],
        pdu_len);
//    printf("PROBE4!\n");
#if PRINT_ENABLED
    if (bytes_sent <= 0) {
        fprintf(stderr, "WP: Failed to send PDU (%s)!\n", strerror(errno));
    }
#else
    bytes_sent = bytes_sent;
#endif

//    printf("PROBE5!\n");

    return;
}
#endif

/** Perform basic validation of Write Property argument based on
 * the assumption that it is a string. Check for correct data type,
 * correct encoding (fixed here as ANSI X34),correct length, and
 * finally if it is allowed to be empty.
 */

bool WPValidateString(
    BACNET_APPLICATION_DATA_VALUE * pValue,
    int iMaxLen,
    bool bEmptyAllowed,
    BACNET_ERROR_CLASS * pErrorClass,
    BACNET_ERROR_CODE * pErrorCode)
{
    bool bResult;

    /* Save on a bit of code duplication by pre selecting the most
     * common outcomes from the tests (not necessarily the most likely
     * outcome of the tests).
     */
    bResult = false;
    *pErrorClass = ERROR_CLASS_PROPERTY;

    if (pValue->tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
        if (characterstring_encoding(&pValue->type.Character_String) ==
            CHARACTER_ANSI_X34) {
            if ((bEmptyAllowed == false) &&
                (characterstring_length(&pValue->type.Character_String) ==
                    0)) {
                *pErrorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            } else if ((bEmptyAllowed == false) &&
                (!characterstring_printable(&pValue->type.Character_String))) {
                /* assumption: non-empty also means must be "printable" */
                *pErrorCode = ERROR_CODE_VALUE_OUT_OF_RANGE;
            } else if (characterstring_length(&pValue->type.Character_String) >
                (uint16_t) iMaxLen) {
                *pErrorClass = ERROR_CLASS_RESOURCES;
                *pErrorCode = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
            } else
                bResult = true; /* It's all good! */
        } else
            *pErrorCode = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
    } else
        *pErrorCode = ERROR_CODE_INVALID_DATA_TYPE;

    return (bResult);
}

/** Perform simple validation of type of Write Property argument based
 * the expected type vs the actual. Set up error response if the
 * validation fails. Cuts out reams of repeated code in the object code.
 */

bool WPValidateArgType(
    BACNET_APPLICATION_DATA_VALUE * pValue,
    uint8_t ucExpectedTag,
    BACNET_ERROR_CLASS * pErrorClass,
    BACNET_ERROR_CODE * pErrorCode)
{
    bool bResult;

    /*
     * start out assuming success and only set up error
     * response if validation fails.
     */
    bResult = true;
    if (pValue->tag != ucExpectedTag) {
        bResult = false;
        *pErrorClass = ERROR_CLASS_PROPERTY;
        *pErrorCode = ERROR_CODE_INVALID_DATA_TYPE;
    }

    return (bResult);
}
