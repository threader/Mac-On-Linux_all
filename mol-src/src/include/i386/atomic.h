/*
 * x86 atomic operations
 */

#ifndef _H_ATOMIC
#define _H_ATOMIC

typedef struct {
	volatile int counter;
} atomic_t;

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		(((v)->counter) = (i))

static inline void
atomic_inc(atomic_t *v)
{
	asm volatile(
		"lock ; incl %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

static inline void
atomic_dec(atomic_t *v)
{
	asm volatile(
		"lock ; decl %0"
		:"=m" (v->counter)
		:"m" (v->counter));
}

/* returns true if the result is non-zero */
static inline int
atomic_inc_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		"lock ; incl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return !c;
}
#define atomic_inc_and_test_(x) atomic_inc_and_test(x)

/* returns true if the result is non-zero */
static __inline__ int
atomic_dec_and_test(atomic_t *v)
{
	unsigned char c;

	__asm__ __volatile__(
		"lock ; decl %0; sete %1"
		:"=m" (v->counter), "=qm" (c)
		:"m" (v->counter) : "memory");
	return !c;
}


#endif   /* _H_ATOMIC */
