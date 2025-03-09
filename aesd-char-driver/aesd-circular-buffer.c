/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Parth Varsani
 * @date 2025-03-08
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer, size_t target_offset, size_t *entry_offset_byte_rtn)
{
    if (buffer == NULL || entry_offset_byte_rtn == NULL)
        return NULL;

    size_t accumulated_bytes = 0;  // Keeps track of total bytes processed
    size_t entries_checked = 0;    // Counts how many entries we have searched
    uint8_t current_index = buffer->out_offs;  // Start from the oldest entry

    while (1)
    {
        // If target_offset falls inside this entry, return the entry
        if (target_offset < (accumulated_bytes + buffer->entry[current_index].size))
        {
            *entry_offset_byte_rtn = target_offset - accumulated_bytes;
            return &buffer->entry[current_index];
        }

        // Move to the next entry in the circular buffer
        accumulated_bytes += buffer->entry[current_index].size;
        current_index = (current_index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        entries_checked++;

        // Stop searching if the buffer is not full and we have checked all written data
        if ((!buffer->full) && (current_index == buffer->in_offs))
        {
            return NULL;
        }

        // Stop searching if we have checked all entries in a full buffer
        if (buffer->full && (entries_checked == AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED))
        {
            return NULL;
        }
    }

    return NULL; // Should never reach here, but included for safety
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    if (buffer == NULL || add_entry == NULL)
        return;

    // If buffer is NOT full, just add the entry and move the write position forward
    if (!(buffer->full))
    {
        buffer->entry[buffer->in_offs] = *add_entry;
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }
    else
    {
        // If buffer is full, overwrite the oldest entry
        buffer->entry[buffer->in_offs] = *add_entry;
        buffer->in_offs = (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
        buffer->out_offs = (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    // If in_offs catches up with out_offs and buffer was not already full, mark it as full
    if ((buffer->in_offs == buffer->out_offs) && !(buffer->full))
    {
        buffer->full = true;
    }
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
