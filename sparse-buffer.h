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
 * \param block The block to store in the buffer.  The buffer makes a copy of
 *              the block's data.
 */
void SparseBuffer_set(SparseBuffer *self, MemBlock block);

/** \brief Add an offset to the position of the data in the buffer.
 *
 * \param self The sparse buffer.
 * \param offset The offset.
 */
void SparseBuffer_offset(SparseBuffer *self, ptrdiff_t offset);

/** \brief Reads data from a sparse buffer.
 *
 * \param self The sparse buffer.
 * \param length The maximum number of bytes to read.  If 0, the next
 *               contiguous block will be read.
 *
 * \return The data.
 */
MemBlock SparseBuffer_read(SparseBuffer *self, size_t length);

/** \brief Get the number of bytes stored in the buffer.
 *
 * \param self The sparse buffer.
 *
 * \return The number of bytes in the buffer, excluding unset gaps.
 */
size_t SparseBuffer_size(SparseBuffer *self);

/** \brief Reset the read position to the beginning of the buffer.
 *
 * \param self The sparse buffer.
 */
void SparseBuffer_rewind(SparseBuffer *self);

/*@}*/

#endif /* STM32SPROG_SPARSE_BUFFER_H */

