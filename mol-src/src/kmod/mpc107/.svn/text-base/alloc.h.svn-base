/* 
 *   Creation Date: <2003/05/27 15:48:37 samuel>
 *   Time-stamp: <2003/05/27 17:43:00 samuel>
 *   
 *	<alloc.h>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_ALLOC
#define _H_ALLOC

#define	spin_lock_mol(x)	do {} while(0)
#define	spin_unlock_mol(x)	do {} while(0)
#define spin_lock_init_mol(x)	do {} while(0)
typedef int			mol_spinlock_t;


extern void 	*kmalloc_mol( int size );
extern void	kfree_mol( void *p );
extern void	*vmalloc_mol( int size );
extern void	vfree_mol( void *p );
extern ulong	alloc_page_mol( void );
extern void	free_page_mol( ulong addr );
extern void	*kmalloc_cont_mol( int size );
extern void	kfree_cont_mol( void *addr );

extern void	flush_icache_mol( ulong start, ulong stop );

extern uint	copy_to_user_mol( void *to, const void *from, ulong len );
extern uint	copy_from_user_mol( void *to, const void *from, ulong len );

static inline ulong tophys_mol( void *addr ) {
	return (ulong)addr;
}

static inline uint copy_int_to_user( int *to, int val ) {
	return copy_to_user_mol( to, &val, sizeof(int) );
}
static inline uint copy_int_from_user( int *retint, int *userptr ) {
	return copy_from_user_mol( retint, userptr, sizeof(int) );
}

/* XXX: to be removed... causes problems with Darwin */
static inline void *phys_to_virt( ulong addr ) {
	return (void*)addr;
}


#endif   /* _H_ALLOC */
