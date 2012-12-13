#ifndef STM32SPROG_SPARSE_BUFFER_H
#define STM32SPROG_SPARSE_BUFFER_H
/** \file sparse-buffer.h */

#include <stddef.h>
#include <stdint.h>

/** \defgroup SparseBufferType SparseBuffer
 * \brief A sparse data buffer.
 * @{
 */

/** \brief Sparse buffer handle. */
typedef struct SparseBuffer SparseBuffer;

/** \brief A memory block. */
typedef struct MemBlock MemBlock;
struct MemBlock {
    /** The offset of the data in memory. */
    size_t offset;
    /** The length of the data in bytes. */
    size_t length;
    /** The data values. */
    const uint8_t *data;
};

/** \brief Sparse buffer constructor.
 *
 * \return A new sparse buffer object, which must be freed by
 *         SparseBuffer_destroy().
 */
SparseBuffer *SparseBuffer_create();

/** \brief Sparse buffer destructor.
 *
 * Cleans up all resources used by a sparse buffer instance.
 *
 * \param self The sparse buffer.
 */
void SparseBuffer_destroy(SparseBuffer *self);

/** \brief Sets data in a sparse buffer.
 *
 * \param self The sparse buffer.
 * \param block The block to store in the buffer.
 */
void SparseBuffer_set(SparseBuffer *self, MemBlock block);

/** \brief Reads data from a sparse buffer.
 *
 * \param self The sparse buffer.
 * \param length The maximum number of bytes to read.
 *
 * \return The data.
 */
MemBlock SparseBuffer_read(SparseBuffer *self, size_t length);

/** \brief Reset the read position to the beginning of the buffer.
 *
 * \param self The sparse buffer.
 */
void SparseBuffer_rewind(SparseBuffer *self);

/*@}*/

#endif /* STM32SPROG_SPARSE_BUFFER_H */

