#include <stdio.h>
#include <string.h>
#include <esp_partition.h>
// Debug buffer size
#define DEBUG_BUFFER_SIZE 130
#define DEBUG false

struct nvs_entry {
  uint8_t  Ns;          // Namespace ID
  uint8_t  Type;        // Type of value
  uint8_t  Span;        // Number of entries used for this item
  uint8_t  Rvs;         // Reserved, should be 0xFF
  uint32_t CRC;         // CRC
  char     Key[16];     // Key in Ascii
  uint64_t Data;        // Data in entry
};

struct nvs_page {        // For nvs entries
  uint32_t  State;
  uint32_t  Seqnr;

  uint32_t  Unused[5];
  uint32_t  CRC;
  uint8_t   Bitmap[32];
  nvs_entry Entry[126];
};

// Common data
nvs_page buf;

//**************************************************************************************************
//                                          D B G P R I N T                                        *
//**************************************************************************************************
// Send a line of info to serial output.  Works like vsprintf(), but checks the DEBUG flag.        *
// Print only if DEBUG flag is true.  Always returns the formatted string.                         *
//**************************************************************************************************
char* dbgprint (const char* format, ...) {
    static char sbuf[DEBUG_BUFFER_SIZE];                // For debug lines
    va_list varArgs;                                    // For variable number of params

    va_start (varArgs, format);                         // Prepare parameters
    vsnprintf (sbuf, sizeof(sbuf), format, varArgs);    // Format the message
    va_end (varArgs);                                   // End of using parameters
    if (DEBUG) {                                        // DEBUG on?
        Serial.print ("D: ");                           // Yes, print prefix
        Serial.println (sbuf);                          // and the info
    }
    return sbuf;                                        // Return stored string
}


//**************************************************************************************************
//                                   F I N D N S I D                                               *
//**************************************************************************************************
// Find the namespace ID for the namespace passed as parameter.                                    *
//**************************************************************************************************
uint8_t FindNsID (const esp_partition_t* nvs, const char* ns) {
  esp_err_t                 result = ESP_OK;                 // Result of reading partition
  uint32_t                  offset = 0;                      // Offset in nvs partition
  uint8_t                   i;                               // Index in Entry 0..125
  uint8_t                   bm;                              // Bitmap for an entry
  uint8_t                   res = 0xFF;                      // Function result


  while (offset < nvs->size) {
    result = esp_partition_read (nvs, offset,                // Read 1 page in nvs partition
                                 &buf,
                                 sizeof(nvs_page));
    if (result != ESP_OK) {
      dbgprint ("Error reading NVS!");
      break;
    }
    i=0;
    while (i < 126) {
      bm = (buf.Bitmap[i/4] >> ((i % 4) * 2)) & 0x03;         // Get bitmap for this entry
      if ((bm == 2) &&
           (buf.Entry[i].Ns == 0) &&
           (strcmp (ns, buf.Entry[i].Key) == 0)) {
        res = buf.Entry[i].Data & 0xFF;                       // Return the ID
        offset = nvs->size;                                   // Stop outer loop as well
        break;
      } else {
        if (bm == 2) {
          i += buf.Entry[i].Span;                             // Next entry
        } else {
          i++;
        }
      }
    }
    offset += sizeof(nvs_page);                               // Prepare to read next page in nvs
  }
  return res;
}