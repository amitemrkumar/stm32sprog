#include "firmware.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

SparseBuffer *readFirmware(const char *fileName, FirmwareFormat *format) {
    if(format && *format != AUTO && *format != RAW) {
        return NULL;
    }

    SparseBuffer *buffer = SparseBuffer_create();
    if(!buffer) goto BufferError;

    FILE *firmware = fopen(fileName, "rb");
    if(!firmware) goto OpenError;

    (void)fseek(firmware, 0L, SEEK_END);
    size_t length = ftell(firmware);
    uint8_t *mem = malloc(length);
    if(!mem) goto AllocError;

    rewind(firmware);
    if(fread(mem, 1, length, firmware) < length) {
        goto ReadError;
    }

    MemBlock block;
    block.offset = 0;
    block.length = length;
    block.data = mem;
    SparseBuffer_set(buffer, block);

    free(mem);
    fclose(firmware);

    if(format) *format = RAW;
    return buffer;

ReadError:
    free(mem);
AllocError:
    fclose(firmware);
OpenError:
    SparseBuffer_destroy(buffer);
BufferError:
    return NULL;
}

