/* 
 *   Creation Date: <2004/01/25 16:34:48 samuel>
 *   Time-stamp: <2004/01/25 21:45:34 samuel>
 *   
 *	<locks.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_LOCKS
#define _H_LOCKS

#include <IOKit/IOLocks.h>

/************** Mutex Locks - Implemented by IOLock **************/

typedef IOLock *mol_mutex_t;

static inline void init_MUTEX_mol( mol_mutex_t *mup ) {
	*mup = IOLockAlloc();
}

static inline void free_MUTEX_mol( mol_mutex_t *mup ) {
	IOLockFree(*mup);
}

static inline void down_mol( mol_mutex_t *mup ) {
	IOLockLock(*mup);
}

static inline void up_mol( mol_mutex_t *mup ) {
	IOLockUnlock(*mup);
}


/************** Spinlocks - Implemented by IOSimpleLock **************/

typedef IOSimpleLock * mol_spinlock_t;

static inline void spin_lock_init_mol (mol_spinlock_t *lock) {
	IOSimpleLockInit(*lock);
}

static inline void spin_lock_mol( mol_spinlock_t *lock ) {
	IOSimpleLockLock(*lock);
}
static inline void spin_unlock_mol( mol_spinlock_t *lock ) {
	IOSimpleLockUnlock(*lock);
}

#endif   /* _H_LOCKS */
