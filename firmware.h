#ifndef STM32SPROG_FIRMWARE_H
#define STM32SPROG_FIRMWARE_H

#include "sparse-buffer.h"

typedef enum {
    /** Detect the file type from its contents. */
    AUTO,
    /** Raw binary. */
    RAW,
    /** Intel HEX. */
    IHEX,
    /** Motorola S-record. */
    SREC
} FirmwareFormat;

/** Read a firmware file into memory.
 *
 * \param[in] fileName The file to read.
 * \param[in,out] format The firmware file format.  If NULL, automatic
 *                       detection will be used.  If AUTO is explicitely
 *                       specified, this will be set to the format detected.
 *
 * \return A new SparseBuffer containing the firmware data.  The caller is
 *         responsible for destroying it.  Returns NULL if the file could not
 *         be read or did not match the specified format.
 */
SparseBuffer *readFirmware(const char *fileName, FirmwareFormat *format);

#endif /* STM32SPROG_FIRMWARE_H */

