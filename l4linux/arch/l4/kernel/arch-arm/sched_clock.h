/*
 * sched_clock.h: support for extending counters to full 64-bit ns counter
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef ASM_SCHED_CLOCK
#define ASM_SCHED_CLOCK

#include <linux/kernel.h>
#include <linux/types.h>

struct clock_data {
	u64 epoch_ns;
	u32 epoch_cyc;
	u32 epoch_cyc_copy;
	u32 mult;
	u32 shift;
};

#define DEFINE_CLOCK_DATA(name)	struct clock_data name

static void sched_clock_poll(unsigned long wrap_ticks);
static DEFINE_TIMER(sched_clock_timer, sched_clock_poll, 0, 0);
static void (*sched_clock_update_fn)(void);

static void sched_clock_poll(unsigned long wrap_ticks)
{
	mod_timer(&sched_clock_timer, round_jiffies(jiffies + wrap_ticks));
	sched_clock_update_fn();
}

static inline u64 cyc_to_ns(u64 cyc, u32 mult, u32 shift)
{
	return (cyc * mult) >> shift;
}

/*
 * Atomically update the sched_clock epoch.  Your update callback will
 * be called from a timer before the counter wraps - read the current
 * counter value, and call this function to safely move the epochs
 * forward.  Only use this from the update callback.
 */
static inline void update_sched_clock(struct clock_data *cd, u32 cyc, u32 mask)
{
	unsigned long flags;
	u64 ns = cd->epoch_ns +
		cyc_to_ns((cyc - cd->epoch_cyc) & mask, cd->mult, cd->shift);

	/*
	 * Write epoch_cyc and epoch_ns in a way that the update is
	 * detectable in cyc_to_fixed_sched_clock().
	 */
	raw_local_irq_save(flags);
	cd->epoch_cyc = cyc;
	smp_wmb();
	cd->epoch_ns = ns;
	smp_wmb();
	cd->epoch_cyc_copy = cyc;
	raw_local_irq_restore(flags);
}

/*
 * If your clock rate is known at compile time, using this will allow
 * you to optimize the mult/shift loads away.  This is paired with
 * init_fixed_sched_clock() to ensure that your mult/shift are correct.
 */
static inline unsigned long long cyc_to_fixed_sched_clock(struct clock_data *cd,
	u32 cyc, u32 mask, u32 mult, u32 shift)
{
	u64 epoch_ns;
	u32 epoch_cyc;

	/*
	 * Load the epoch_cyc and epoch_ns atomically.  We do this by
	 * ensuring that we always write epoch_cyc, epoch_ns and
	 * epoch_cyc_copy in strict order, and read them in strict order.
	 * If epoch_cyc and epoch_cyc_copy are not equal, then we're in
	 * the middle of an update, and we should repeat the load.
	 */
	do {
		epoch_cyc = cd->epoch_cyc;
		smp_rmb();
		epoch_ns = cd->epoch_ns;
		smp_rmb();
	} while (epoch_cyc != cd->epoch_cyc_copy);

	return epoch_ns + cyc_to_ns((cyc - epoch_cyc) & mask, mult, shift);
}

/*
 * Otherwise, you need to use this, which will obtain the mult/shift
 * from the clock_data structure.  Use init_sched_clock() with this.
 */
static inline unsigned long long cyc_to_sched_clock(struct clock_data *cd,
	u32 cyc, u32 mask)
{
	return cyc_to_fixed_sched_clock(cd, cyc, mask, cd->mult, cd->shift);
}

/*
 * Initialize the clock data - calculate the appropriate multiplier
 * and shift.  Also setup a timer to ensure that the epoch is refreshed
 * at the appropriate time interval, which will call your update
 * handler.
 */
void __init init_sched_clock(struct clock_data *cd, void (*update)(void),
	unsigned int clock_bits, unsigned long rate)
{
	unsigned long r, w;
	u64 res, wrap;
	char r_unit;

	sched_clock_update_fn = update;

	/* calculate the mult/shift to convert counter ticks to ns. */
	clocks_calc_mult_shift(&cd->mult, &cd->shift, rate, NSEC_PER_SEC, 0);

	r = rate;
	if (r >= 4000000) {
		r /= 1000000;
		r_unit = 'M';
	} else {
		r /= 1000;
		r_unit = 'k';
	}

	/* calculate how many ns until we wrap */
	wrap = cyc_to_ns((1ULL << clock_bits) - 1, cd->mult, cd->shift);
	do_div(wrap, NSEC_PER_MSEC);
	w = wrap;

	/* calculate the ns resolution of this counter */
	res = cyc_to_ns(1ULL, cd->mult, cd->shift);
	pr_info("sched_clock: %u bits at %lu%cHz, resolution %lluns, wraps every %lums\n",
		clock_bits, r, r_unit, res, w);

	/*
	 * Start the timer to keep sched_clock() properly updated and
	 * sets the initial epoch.
	 */
	sched_clock_timer.data = msecs_to_jiffies(w - (w / 10));
	update();

	/*
	 * Ensure that sched_clock() starts off at 0ns
	 */
	cd->epoch_ns = 0;
}

/*
 * Use this initialization function rather than init_sched_clock() if
 * you're using cyc_to_fixed_sched_clock, which will warn if your
 * constants are incorrect.
 */
static inline void init_fixed_sched_clock(struct clock_data *cd,
	void (*update)(void), unsigned int bits, unsigned long rate,
	u32 mult, u32 shift)
{
	init_sched_clock(cd, update, bits, rate);
	if (cd->mult != mult || cd->shift != shift) {
		pr_crit("sched_clock: wrong multiply/shift: %u>>%u vs calculated %u>>%u\n"
			"sched_clock: fix multiply/shift to avoid scheduler hiccups\n",
			mult, shift, cd->mult, cd->shift);
	}
}

extern void sched_clock_postinit(void);

#endif
