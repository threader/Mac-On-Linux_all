/*
 * PowerPC atomic operations
 */

#ifndef _H_ATOMIC
#define _H_ATOMIC

typedef struct {
	volatile int counter; 
} atomic_t;

#define atomic_read(v)		((v)->counter)
#define atomic_set(v,i)		(((v)->counter) = (i))

static inline int
atomic_dec_and_test( atomic_t *v )
{
	int t;

	asm volatile(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b\n\
	isync\n"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc" );

	return t;
}

static inline int
atomic_inc_and_test( atomic_t *v )
{
	int t;

	asm volatile(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b\n\
	isync\n"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc" );

	return t;
}

/* faster, but without the trailing isync */
static inline int
atomic_inc_and_test_( atomic_t *v )
{
	int t;

	asm volatile(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b\n"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc" );

	return t;
}

static inline void
atomic_inc( atomic_t *v )
{
	int t;

	asm volatile(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b\n"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc" );
}

static inline void 
atomic_dec( atomic_t *v )
{
	int t;

	asm volatile(
"1:	lwarx	%0,0,%2\n\
	addic	%0,%0,-1\n\
	stwcx.	%0,0,%2\n\
	bne-	1b\n"
	: "=&r" (t), "=m" (v->counter)
	: "r" (&v->counter), "m" (v->counter)
	: "cc" );
}

#endif   /* _H_ATOMIC */
