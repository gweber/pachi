#ifndef PACHI_PATTERN3_H
#define PACHI_PATTERN3_H

#include "board.h"

#define PAT3_SHORT_CIRCUIT          /* Speedup when no stones around */


/* Fast matching of simple 3x3 patterns. */

/* (Note that this is completely independent from the general pattern
 * matching infrastructure in pattern.[ch]. This is fast and simple.) */

/* hash3_t pattern: ignore middle point, 2 bits per intersection (color)
 * plus 1 bit per each direct neighbor => 8*2 + 4 bits. Bitmap point order:
 * 7 6 5    b
 * 4   3  a   9
 * 2 1 0    8   */
/* Value bit 0: black pattern; bit 1: white pattern */

/* XXX: See <board.h> for hash3_t typedef. */

typedef struct {
	/* The 3x3 pattern code (hash3_t, see pattern3_hash()) is only 20 bits
	 * wide (16 color bits + 4 atari bits), so we use it to directly index
	 * this table rather than hashing into a sparse one. value == 0 means
	 * "no pattern matches here"; otherwise bit 0/1 = pattern matches for
	 * black/white and (value >> 2) = pattern index (used to look up gamma).
	 * Direct indexing avoids both the zobrist hash and the open-addressing
	 * probe; the table is also 4x smaller (1MB) than the old hash table so
	 * it stays cache-resident during playouts. */
#define pattern3_table_bits 20
#define pattern3_table_size (1 << pattern3_table_bits)
	unsigned char value[pattern3_table_size];
} pattern3s_t;

/* Source pattern encoding:
 * X: black;  O: white;  .: empty;  #: edge
 * x: !black; o: !white; ?: any
 *
 * |/=: black in atari/anything but black in atari
 * @/0: white in atari
 * Y/y: black notin atari; Q/q: white notin atari
 *
 * extra X: pattern valid only for one side;
 * middle point ignored. */

void pattern3s_init(pattern3s_t *p, char src[][11], int src_n);

/* Compute pattern3 hash at local position. */
static hash3_t pattern3_hash(board_t *b, coord_t c);

/* Check if we match any 3x3 pattern centered on given move. */
static bool pattern3_move_here(pattern3s_t *p, board_t *b, move_t *m, char *idx);

/* Generate all transpositions of given pattern, stored in an
 * hash3_t[8] array. */
void pattern3_transpose(hash3_t pat, hash3_t (*transp)[8]);

/* Reverse pattern to opposite color assignment. */
static hash3_t pattern3_reverse(hash3_t pat);


static inline hash3_t
pattern3_hash(board_t *b, coord_t c)
{
	int stride = board_stride(b);

        int c1 = c - stride - 1;
        int c2 = c - stride    ;
        int c3 = c - stride + 1;
        int c4 = c - 1;
        int c5 = c + 1;
        int c6 = c + stride - 1;
        int c7 = c + stride    ;
        int c8 = c + stride + 1;

        /* Stone and atari info. */
#define atari_at(b, c) (group_at(b, c) && group_libs(b, group_at(b, c)) == 1)
        return ((board_at(b, c1) << 14) |
                (board_at(b, c2) << 12) |
                (board_at(b, c3) << 10) |
                (board_at(b, c4) << 8)  |
                (board_at(b, c5) << 6)  |
                (board_at(b, c6) << 4)  |
                (board_at(b, c7) << 2)  |
                (board_at(b, c8)     )  |
                (atari_at(b, c2) << 19) |
                (atari_at(b, c4) << 18) |
                (atari_at(b, c5) << 17) |
                (atari_at(b, c7) << 16));
#undef atari_at
}

static inline bool
pattern3_move_here(pattern3s_t *p, board_t *b, move_t *m, char *idx)
{
	coord_t c = m->coord;
	int stride = board_stride(b);
	int c1 = board_at(b, c - stride - 1);
	int c3 = board_at(b, c - stride + 1);
	int c6 = board_at(b, c + stride - 1);
	int c8 = board_at(b, c + stride + 1);

#ifdef PAT3_SHORT_CIRCUIT
	/* Nothing can match if there's no black stones or no white stones around. */
	if (!(neighbor_count_at(b, c, S_BLACK) || (c1 == S_BLACK) || (c3 == S_BLACK) ||  (c6 == S_BLACK) ||  (c8 == S_BLACK)) ||
	    !(neighbor_count_at(b, c, S_WHITE) || (c1 == S_WHITE) || (c3 == S_WHITE) ||  (c6 == S_WHITE) ||  (c8 == S_WHITE)) )
		return false;
#endif

#ifdef BOARD_PAT3
	hash3_t pat = b->pat3[m->coord];
#else
	hash3_t pat = pattern3_hash(b, m->coord);
#endif
	/* Direct table lookup: pat is a 20-bit code (see pattern3s_t). */
	unsigned char value = p->value[pat];
	if (value & m->color) {
		*idx = value >> 2;
		return true;
	}

	return false;
}

static inline hash3_t
pattern3_reverse(hash3_t pat)
{
	/* Reverse color assignment - achieved by swapping odd and even bits */
	return ((pat >> 1) & 0x5555) | ((pat & 0x5555) << 1) | (pat & 0xf0000);
}

#endif
