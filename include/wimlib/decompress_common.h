/*
 * decompress_common.h
 *
 * Header for decompression code shared by multiple compression formats.
 *
 * The following copying information applies to this specific source code file:
 *
 * Written in 2012-2016 by Eric Biggers <ebiggers3@gmail.com>
 *
 * To the extent possible under law, the author(s) have dedicated all copyright
 * and related and neighboring rights to this software to the public domain
 * worldwide via the Creative Commons Zero 1.0 Universal Public Domain
 * Dedication (the "CC0").
 *
 * This software is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the CC0 for more details.
 *
 * You should have received a copy of the CC0 along with this software; if not
 * see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#ifndef _WIMLIB_DECOMPRESS_COMMON_H
#define _WIMLIB_DECOMPRESS_COMMON_H

#include <string.h>

#include "wimlib/compiler.h"
#include "wimlib/types.h"
#include "wimlib/unaligned.h"

/* Structure that encapsulates a block of in-memory data being interpreted as a
 * stream of bits, optionally with interwoven literal bytes.  Bits are assumed
 * to be stored in little endian 16-bit coding units, with the bits ordered high
 * to low.  */
struct input_bitstream {

	/* Bits that have been read from the input buffer.  The bits are
	 * left-justified; the next bit is always bit 31.  */
	machine_word_t bitbuf;

	/* Number of bits currently held in @bitbuf.  */
	machine_word_t bitsleft;

	/* Pointer to the next byte to be retrieved from the input buffer.  */
	const u8 *next;

	/* Pointer past the end of the input buffer.  */
	const u8 *end;
};

/* Initialize a bitstream to read from the specified input buffer.  */
static inline void
init_input_bitstream(struct input_bitstream *is, const void *buffer, u32 size)
{
	is->bitbuf = 0;
	is->bitsleft = 0;
	is->next = buffer;
	is->end = is->next + size;
}

/* Note: for performance reasons, the following methods don't return error codes
 * to the caller if the input buffer is overrun.  Instead, they just assume that
 * all overrun data is zeroes.  This has no effect on well-formed compressed
 * data.  The only disadvantage is that bad compressed data may go undetected,
 * but even this is irrelevant if higher level code checksums the uncompressed
 * data anyway.  */

/* Ensure the bit buffer variable for the bitstream contains at least @num_bits
 * bits.  Following this, bitstream_peek_bits() and/or bitstream_remove_bits()
 * may be called on the bitstream to peek or remove up to @num_bits bits.  */
static inline void
bitstream_ensure_bits(struct input_bitstream *is, const unsigned num_bits)
{
	/* This currently works for at most 17 bits.  */

	if (is->bitsleft >= num_bits)
		return;

	/*if (unlikely(is->end - is->next < 6))*/
		/*goto slow;*/

	/*is->bitbuf |= (machine_word_t)get_unaligned_le16(is->next + 0) << (WORDBITS - 16 - is->bitsleft);*/
	/*is->bitbuf |= (machine_word_t)get_unaligned_le16(is->next + 2) << (WORDBITS - 32 - is->bitsleft);*/
	/*is->bitbuf |= (machine_word_t)get_unaligned_le16(is->next + 4) << (WORDBITS - 48 - is->bitsleft);*/
	/*is->next += 6;*/
	/*is->bitsleft += 48;*/

	/*return;*/

/*slow:*/
	if (likely(is->end - is->next >= 2)) {
		is->bitbuf |=
			(machine_word_t)get_unaligned_le16(is->next) <<
			(WORDBITS - 16 - is->bitsleft);
		is->next += 2;
	}
	is->bitsleft += 16;
	if (unlikely(num_bits > 16 && is->bitsleft < num_bits)) {
		if (likely(is->end - is->next >= 2)) {
			is->bitbuf |=
				(machine_word_t)get_unaligned_le16(is->next) <<
				(WORDBITS - 16 - is->bitsleft);
			is->next += 2;
		}
		is->bitsleft += 16;
		if (unlikely(num_bits > 32 && is->bitsleft < num_bits)) {
			if (likely(is->end - is->next >= 2)) {
				is->bitbuf |=
					(machine_word_t)get_unaligned_le16(is->next) <<
					(WORDBITS - 16 - is->bitsleft);
				is->next += 2;
			}
			is->bitsleft += 16;
		}
	}
}

/* Return the next @num_bits bits from the bitstream, without removing them.
 * There must be at least @num_bits remaining in the buffer variable, from a
 * previous call to bitstream_ensure_bits().  */
static inline u32
bitstream_peek_bits(const struct input_bitstream *is, const unsigned num_bits)
{
	return (is->bitbuf >> 1) >> (WORDBITS - num_bits - 1);
}

/* Remove @num_bits from the bitstream.  There must be at least @num_bits
 * remaining in the buffer variable, from a previous call to
 * bitstream_ensure_bits().  */
static inline void
bitstream_remove_bits(struct input_bitstream *is, unsigned num_bits)
{
	is->bitbuf <<= num_bits;
	is->bitsleft -= num_bits;
}

/* Remove and return @num_bits bits from the bitstream.  There must be at least
 * @num_bits remaining in the buffer variable, from a previous call to
 * bitstream_ensure_bits().  */
static inline u32
bitstream_pop_bits(struct input_bitstream *is, unsigned num_bits)
{
	u32 bits = bitstream_peek_bits(is, num_bits);
	bitstream_remove_bits(is, num_bits);
	return bits;
}

/* Read and return the next @num_bits bits from the bitstream.  */
static inline u32
bitstream_read_bits(struct input_bitstream *is, unsigned num_bits)
{
	bitstream_ensure_bits(is, num_bits);
	return bitstream_pop_bits(is, num_bits);
}

/* Read and return the next literal byte embedded in the bitstream.  */
static inline u8
bitstream_read_byte(struct input_bitstream *is)
{
	if (unlikely(is->end == is->next))
		return 0;
	return *is->next++;
}

/* Read and return the next 16-bit integer embedded in the bitstream.  */
static inline u16
bitstream_read_u16(struct input_bitstream *is)
{
	u16 v;

	if (unlikely(is->end - is->next < 2))
		return 0;
	v = get_unaligned_le16(is->next);
	is->next += 2;
	return v;
}

/* Read and return the next 32-bit integer embedded in the bitstream.  */
static inline u32
bitstream_read_u32(struct input_bitstream *is)
{
	u32 v;

	if (unlikely(is->end - is->next < 4))
		return 0;
	v = get_unaligned_le32(is->next);
	is->next += 4;
	return v;
}

/* Read into @dst_buffer an array of literal bytes embedded in the bitstream.
 * Return 0 if there were enough bytes remaining in the input, otherwise -1. */
static inline int
bitstream_read_bytes(struct input_bitstream *is, void *dst_buffer, size_t count)
{
	if (unlikely(is->end - is->next < count))
		return -1;
	memcpy(dst_buffer, is->next, count);
	is->next += count;
	return 0;
}

/* Align the input bitstream on a coding-unit boundary.  */
static inline void
bitstream_align(struct input_bitstream *is)
{
	is->bitsleft = 0;
	is->bitbuf = 0;
}

/* Needed alignment of decode_table parameter to make_huffman_decode_table().
 *
 * Reason: We may fill the entries with SSE instructions without worrying
 * about dealing with the unaligned case.  */
#define DECODE_TABLE_ALIGNMENT 16

#define DECODE_TABLE_SYMBOL_SHIFT 4
#define DECODE_TABLE_LENGTH_MASK DECODE_TABLE_MAX_LENGTH

#define DECODE_TABLE_MAX_NUM_SYMS ((1 << (16 - DECODE_TABLE_SYMBOL_SHIFT)) - 1)
#define DECODE_TABLE_MAX_LENGTH ((1 << DECODE_TABLE_SYMBOL_SHIFT) - 1)

/*
 * Reads and returns the next Huffman-encoded symbol from a bitstream.
 *
 * If the input data is exhausted, the Huffman symbol is decoded as if the
 * missing bits are all zeroes.
 *
 * XXX: This is mostly duplicated in lzms_decode_huffman_symbol() in
 * lzms_decompress.c.
 */
static inline unsigned
pop_huffsym(struct input_bitstream *is, const u16 decode_table[],
	    unsigned table_bits, unsigned max_codeword_len)
{
	unsigned entry;
	unsigned sym;
	unsigned len;

	/* Index the root table by the next 'table_bits' bits of input. */
	entry = decode_table[bitstream_peek_bits(is, table_bits)];

	/* Extract the symbol and length from the entry. */
	sym = entry >> DECODE_TABLE_SYMBOL_SHIFT;
	len = entry & DECODE_TABLE_LENGTH_MASK;

	/* If the root table is indexed by the full 'max_codeword_len' bits,
	 * then there cannot be any subtables.  This will be known at compile
	 * time.  Otherwise, we must check whether the decoded symbol is really
	 * a subtable pointer.  If so, we must discard the bits with which the
	 * root table was indexed, then index the subtable by the next 'len'
	 * bits of input to get the real entry. */
	if (max_codeword_len > table_bits &&
	    entry >= (1U << (table_bits + DECODE_TABLE_SYMBOL_SHIFT)))
	{
		/* Subtable required */
		bitstream_remove_bits(is, table_bits);
		entry = decode_table[sym + bitstream_peek_bits(is, len)];
		sym = entry >> DECODE_TABLE_SYMBOL_SHIFT;;
		len = entry & DECODE_TABLE_LENGTH_MASK;
	}

	/* Discard the bits (or the remaining bits, if a subtable was required)
	 * of the codeword. */
	bitstream_remove_bits(is, len);

	/* Return the decoded symbol. */
	return sym;
}

/*
 * The ENOUGH() macro returns the maximum number of decode table entries,
 * including all subtable entries, that may be required for decoding a given
 * Huffman code.  This depends on three parameters:
 *
 *	num_syms: the maximum number of symbols in the code
 *	table_bits: the number of bits with which the root table will be indexed
 *	max_codeword_len: the maximum allowed codeword length
 *
 * Given these parameters, the utility program 'enough' from zlib, when run as
 * './enough num_syms table_bits max_codeword_len', will compute the maximum
 * number of entries required.  This has already been done for the combinations
 * we need (or may need) and incorporated into the macro below so that the
 * mapping can be done at compilation time.  If an unknown combination is used,
 * then a compilation error will result.  To fix this, use 'enough' to find the
 * missing value and add it below.
 */
#define ENOUGH(num_syms, table_bits, max_codeword_len) ( \
	((num_syms) == 8 && (table_bits) == 7 && (max_codeword_len) == 15) ? 128 : \
	((num_syms) == 8 && (table_bits) == 5 && (max_codeword_len) == 7) ? 36 : \
	((num_syms) == 8 && (table_bits) == 6 && (max_codeword_len) == 7) ? 66 : \
	((num_syms) == 8 && (table_bits) == 7 && (max_codeword_len) == 7) ? 128 : \
	((num_syms) == 20 && (table_bits) == 5 && (max_codeword_len) == 15) ? 1062 : \
	((num_syms) == 20 && (table_bits) == 6 && (max_codeword_len) == 15) ? 582 : \
	((num_syms) == 20 && (table_bits) == 7 && (max_codeword_len) == 15) ? 390 : \
	((num_syms) == 54 && (table_bits) == 9 && (max_codeword_len) == 15) ? 618 : \
	((num_syms) == 54 && (table_bits) == 10 && (max_codeword_len) == 15) ? 1098 : \
	((num_syms) == 249 && (table_bits) == 9 && (max_codeword_len) == 16) ? 878 : \
	((num_syms) == 249 && (table_bits) == 10 && (max_codeword_len) == 16) ? 1326 : \
	((num_syms) == 249 && (table_bits) == 11 && (max_codeword_len) == 16) ? 2318 : \
	((num_syms) == 256 && (table_bits) == 9 && (max_codeword_len) == 15) ? 822 : \
	((num_syms) == 256 && (table_bits) == 10 && (max_codeword_len) == 15) ? 1302 : \
	((num_syms) == 256 && (table_bits) == 11 && (max_codeword_len) == 15) ? 2310 : \
	((num_syms) == 512 && (table_bits) == 10 && (max_codeword_len) == 15) ? 1558 : \
	((num_syms) == 512 && (table_bits) == 11 && (max_codeword_len) == 15) ? 2566 : \
	((num_syms) == 512 && (table_bits) == 12 && (max_codeword_len) == 15) ? 4606 : \
	((num_syms) == 656 && (table_bits) == 10 && (max_codeword_len) == 16) ? 1734 : \
	((num_syms) == 656 && (table_bits) == 11 && (max_codeword_len) == 16) ? 2726 : \
	((num_syms) == 656 && (table_bits) == 12 && (max_codeword_len) == 16) ? 4758 : \
	((num_syms) == 799 && (table_bits) == 9 && (max_codeword_len) == 15) ? 1366 : \
	((num_syms) == 799 && (table_bits) == 10 && (max_codeword_len) == 15) ? 1846 : \
	((num_syms) == 799 && (table_bits) == 11 && (max_codeword_len) == 15) ? 2854 : \
	-1)

/* Wrapper around ENOUGH() that does additional compile-time validation. */
#define DECODE_TABLE_LENGTH(num_syms, table_bits, max_codeword_len) (	\
									\
	/* Every possible symbol value must fit into the symbol portion	\
	 * of a decode table entry. */					\
	STATIC_ASSERT_ZERO((num_syms) <= DECODE_TABLE_MAX_NUM_SYMS) +	\
									\
	/* There cannot be more symbols than possible codewords. */	\
	STATIC_ASSERT_ZERO((num_syms) <= 1U << (max_codeword_len)) +	\
									\
	/* It doesn't make sense to use a table_bits more than the	\
	 * maximum codeword length. */					\
	STATIC_ASSERT_ZERO((max_codeword_len) >= (table_bits)) +	\
									\
	/* The maximum length in the root table	must fit into the	\
	 * length portion of a decode table entry. */			\
	STATIC_ASSERT_ZERO((table_bits) <= DECODE_TABLE_MAX_LENGTH) +	\
									\
	/* The maximum length in a subtable must fit into the length
	 * portion of a decode table entry. */				\
	STATIC_ASSERT_ZERO((max_codeword_len) - (table_bits) <=		\
					DECODE_TABLE_MAX_LENGTH) +	\
									\
	/* The needed 'enough' value must have been defined. */		\
	STATIC_ASSERT_ZERO(ENOUGH((num_syms), (table_bits),		\
				  (max_codeword_len)) >= 0) +		\
									\
	/* The maximum subtable index must fit in the field which would	\
	 * normally hold a symbol value. */				\
	STATIC_ASSERT_ZERO(ENOUGH((num_syms), (table_bits),		\
				  (max_codeword_len)) <=		\
					DECODE_TABLE_MAX_NUM_SYMS) +	\
									\
	/* The minimum subtable index must be greater than the greatest	\
	 * possible symbol value. */					\
	STATIC_ASSERT_ZERO((1U << table_bits) >= num_syms) +		\
									\
	ENOUGH(num_syms, table_bits, max_codeword_len)			\
)

/*
 * Declare the decode table for a Huffman code, given several compile-time
 * constants that describe that code (see ENOUGH() for details).
 *
 * Decode tables must be aligned to a DECODE_TABLE_ALIGNMENT-boundary.  This
 * implies that if a decode table is nested a dynamically allocated structure,
 * then the outer structure must be allocated on a DECODE_TABLE_ALIGNMENT-byte
 * boundary as well.
 */
#define DECODE_TABLE(name, num_syms, table_bits, max_codeword_len) \
	u16 name[DECODE_TABLE_LENGTH((num_syms), (table_bits), \
				     (max_codeword_len))] \
		_aligned_attribute(DECODE_TABLE_ALIGNMENT)

extern int
make_huffman_decode_table(u16 decode_table[], unsigned num_syms,
			  unsigned table_bits, const u8 lens[],
			  unsigned max_codeword_len);

static inline void
copy_word_unaligned(const void *src, void *dst)
{
	store_word_unaligned(load_word_unaligned(src), dst);
}

static inline machine_word_t
repeat_u16(u16 b)
{
	machine_word_t v = b;

	STATIC_ASSERT(WORDBITS == 32 || WORDBITS == 64);
	v |= v << 16;
	v |= v << ((WORDBITS == 64) ? 32 : 0);
	return v;
}

static inline machine_word_t
repeat_u8(u8 b)
{
	return repeat_u16(((u16)b << 8) | b);
}

/*
 * Copy an LZ77 match at (dst - offset) to dst.
 *
 * The length and offset must be already validated --- that is, (dst - offset)
 * can't underrun the output buffer, and (dst + length) can't overrun the output
 * buffer.  Also, the length cannot be 0.
 *
 * @winend points to the byte past the end of the output buffer.
 * This function won't write any data beyond this position.
 */
static inline void
lz_copy(u8 *dst, u32 length, u32 offset, const u8 *winend, u32 min_length)
{
	const u8 *src = dst - offset;
	const u8 * const end = dst + length;

	/*
	 * Try to copy one machine word at a time.  On i386 and x86_64 this is
	 * faster than copying one byte at a time, unless the data is
	 * near-random and all the matches have very short lengths.  Note that
	 * since this requires unaligned memory accesses, it won't necessarily
	 * be faster on every architecture.
	 *
	 * Also note that we might copy more than the length of the match.  For
	 * example, if a word is 8 bytes and the match is of length 5, then
	 * we'll simply copy 8 bytes.  This is okay as long as we don't write
	 * beyond the end of the output buffer, hence the check for (winend -
	 * end >= WORDBYTES - 1).
	 */
	if (UNALIGNED_ACCESS_IS_FAST && likely(winend - end >= WORDBYTES - 1)) {

		if (offset >= WORDBYTES) {
			/* The source and destination words don't overlap.  */

			/* To improve branch prediction, one iteration of this
			 * loop is unrolled.  Most matches are short and will
			 * fail the first check.  But if that check passes, then
			 * it becomes increasing likely that the match is long
			 * and we'll need to continue copying.  */

			copy_word_unaligned(src, dst);
			src += WORDBYTES;
			dst += WORDBYTES;

			if (dst < end) {
				do {
					copy_word_unaligned(src, dst);
					src += WORDBYTES;
					dst += WORDBYTES;
				} while (dst < end);
			}
			return;
		} else if (offset == 1) {

			/* Offset 1 matches are equivalent to run-length
			 * encoding of the previous byte.  This case is common
			 * if the data contains many repeated bytes.  */

			machine_word_t v = repeat_u8(*(dst - 1));
			do {
				store_word_unaligned(v, dst);
				src += WORDBYTES;
				dst += WORDBYTES;
			} while (dst < end);
			return;
		}
		/*
		 * We don't bother with special cases for other 'offset <
		 * WORDBYTES', which are usually rarer than 'offset == 1'.
		 * Extra checks will just slow things down.  Actually, it's
		 * possible to handle all the 'offset < WORDBYTES' cases using
		 * the same code, but it still becomes more complicated doesn't
		 * seem any faster overall; it definitely slows down the more
		 * common 'offset == 1' case.
		 */
	}

	/* Fall back to a bytewise copy.  */

	if (min_length >= 2) {
		*dst++ = *src++;
		length--;
	}
	if (min_length >= 3) {
		*dst++ = *src++;
		length--;
	}
	if (min_length >= 4) {
		*dst++ = *src++;
		length--;
	}
	do {
		*dst++ = *src++;
	} while (--length);
}

#endif /* _WIMLIB_DECOMPRESS_COMMON_H */
