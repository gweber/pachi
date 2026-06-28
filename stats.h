#ifndef PACHI_STATS_H
#define PACHI_STATS_H

#include <math.h>

/* Move statistics; we track how good value each move has. */
/* These operations are supposed to be atomic - reasonably
 * safe to perform by multiple threads at once on the same stats.
 * What this means in practice is that perhaps the value will get
 * slightly wrong, but not drastically corrupted. */

typedef struct {
	floating_t value; // BLACK wins/playouts
	int playouts; // # of playouts
} move_stats_t;

#define move_stats(value, playouts)  { value, playouts }

/* Add a result to the stats. */
static void stats_add_result(move_stats_t *s, floating_t result, int playouts);

/* Remove a result from the stats. */
static void stats_rm_result(move_stats_t *s, floating_t result, int playouts);

/* Merge two stats together. THIS IS NOT ATOMIC! */
static void stats_merge(move_stats_t *dest, move_stats_t *src);

/* Reverse stats parity. */
static void stats_reverse_parity(move_stats_t *s);


/* We actually do the atomicity in a pretty hackish way - we simply
 * rely on the fact that int,floating_t operations should be atomic with
 * reasonable compilers (gcc) on reasonable architectures (i386,
 * x86_64). */
/* There is a write order dependency - when we bump the playouts,
 * our value must be already correct, otherwise the node will receive
 * invalid evaluation if that's made in parallel, esp. when
 * current s->playouts is zero. */

static inline void
stats_add_result(move_stats_t *s, floating_t result, int playouts)
{
	int s_playouts = s->playouts;
	floating_t s_value = s->value;

	s_playouts += playouts;
	s_value += (result - s_value) * playouts / s_playouts;

	/* We rely on the fact that these two assignments are atomic.
	 * Publish the value before the playout count with a store-release:
	 * a concurrent reader that sees the new count is then guaranteed to
	 * also see the matching value (otherwise a node could be evaluated
	 * with a raised count but a stale/zero value). On x86 (TSO) this
	 * compiles to a plain store; on weakly-ordered archs (ARM) it's a
	 * cheap store-release instead of the two full memory barriers we
	 * used to emit on every stats update. */
	s->value = s_value;
	__atomic_store_n(&s->playouts, s_playouts, __ATOMIC_RELEASE);
}

static inline void
stats_rm_result(move_stats_t *s, floating_t result, int playouts)
{
	if (s->playouts > playouts) {
		int s_playouts = s->playouts;
		floating_t s_value = s->value;

		s_playouts -= playouts;
		s_value += (s_value - result) * playouts / s_playouts;

		/* Publish value before the playout count (store-release), as in
		 * stats_add_result(). */
		s->value = s_value;
		__atomic_store_n(&s->playouts, s_playouts, __ATOMIC_RELEASE);

	} else {
		/* We don't touch the value, since in parallel, another
		 * thread can be adding a result, thus raising the
		 * playouts count after we zero the value. Instead,
		 * leaving the value as is with zero playouts should
		 * not break anything. */
		s->playouts = 0;
	}
}

static inline void
stats_merge(move_stats_t *dest, move_stats_t *src)
{
	/* In a sense, this is non-atomic version of stats_add_result(). */
	if (src->playouts) {
		dest->playouts += src->playouts;
		dest->value += (src->value - dest->value) * src->playouts / dest->playouts;
	}
}

static inline void
stats_reverse_parity(move_stats_t *s)
{
	s->value = 1 - s->value;
}

#endif
