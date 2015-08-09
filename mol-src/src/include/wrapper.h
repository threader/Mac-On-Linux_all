/* 
 *   Creation Date: <2003/01/26 03:02:19 samuel>
 *   Time-stamp: <2004/03/13 16:38:35 samuel>
 *   
 *	<wrapper.h>
 *	
 *	syscall interface
 *   
 *   Copyright (C) 1997, 1999-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_WRAPPER
#define _H_WRAPPER


#include <sys/types.h>
#include <sys/ioctl.h>

#include "prom.h"
#include "mac_registers.h"
#include "mol-ioctl.h"
#include "mmu_mappings.h"
#include "constants.h"

extern void	wrapper_init( void );
extern void	wrapper_cleanup( void );

extern int	mol_ioctl( int cmd, int p1, int p2, int p3 );
extern int	mol_ioctl_simple( int cmd );

/* mol_ioctl_pb calling conventions */

static inline int _smp_send_ipi( void ) {
	return mol_ioctl_simple( MOL_IOCTL_SMP_SEND_IPI ); }

static inline int _create_session( int sessnum ) { 
	return mol_ioctl( MOL_IOCTL_CREATE_SESSION, sessnum, 0, 0 ); }

static inline int _get_info( mol_kmod_info_t *retinfo ) {
	return mol_ioctl( MOL_IOCTL_GET_INFO, (int)retinfo, sizeof(*retinfo), 0 ); }

static inline int _debugger_op( dbg_op_params_t *pb ) {
	return mol_ioctl( MOL_IOCTL_DEBUGGER_OP, (int)pb, 0, 0 ); }

static inline int _mmu_map( struct mmu_mapping *m, int add ) {
	return mol_ioctl( MOL_IOCTL_MMU_MAP, (int)m, add, 0 ); }

static inline int _copy_last_rompage( char *destpage ) {
	return mol_ioctl( MOL_IOCTL_COPY_LAST_ROMPAGE, (int)destpage, 0, 0 ); }

static inline int _setup_fb_accel( char *lvbase, int bytes_per_row, int height ) {
	return mol_ioctl( MOL_IOCTL_SETUP_FBACCEL, (int)lvbase, bytes_per_row, height); }

static inline int _get_dirty_fb_lines( short *rettable, int table_size_in_bytes ) {
	return mol_ioctl( MOL_IOCTL_GET_DIRTY_FBLINES, (int)rettable, table_size_in_bytes, 0 ); }

static inline int _get_performance_info( int index, perf_ctr_t *r ) {
	return mol_ioctl( MOL_IOCTL_GET_PERF_INFO, index, (int)r, 0 ); }

static inline uint _get_session_magic( uint random_magic ) { 
	return mol_ioctl( MOL_IOCTL_GET_SESSION_MAGIC, random_magic, 0, 0 ); }

static inline int _spr_changed( void ) {
	return mol_ioctl( MOL_IOCTL_SPR_CHANGED, 0, 0, 0 ); }

static inline int _set_ram( ulong lv_base, ulong lv_size ) {
	return mol_ioctl( MOL_IOCTL_SET_RAM, lv_base, lv_size, 0 ); }

static inline int _add_io_range( ulong mbase, int size, void *usr_data ) { 
	return mol_ioctl( MOL_IOCTL_ADD_IORANGE, mbase, size, (int)usr_data ); }

static inline int _remove_io_range( ulong mbase, int size ) {
	return mol_ioctl( MOL_IOCTL_REMOVE_IORANGE, mbase, size, 0 ); }

static inline int _alloc_emuaccel_slot( int emuaccel_flags, int param, int inst_addr ) {
	return mol_ioctl( MOL_IOCTL_ALLOC_EMUACCEL_SLOT, emuaccel_flags, param, inst_addr ); }

static inline int _mapin_emuaccel_page( int mphys ) {
	return mol_ioctl( MOL_IOCTL_MAPIN_EMUACCEL_PAGE, mphys, 0, 0 ); }

static inline int _idle_reclaim_memory( void ) {
	return mol_ioctl( MOL_IOCTL_IDLE_RECLAIM_MEMORY, 0, 0, 0 ); }

static inline int _clear_performance_info( void ) {
	return mol_ioctl( MOL_IOCTL_CLEAR_PERF_INFO, 0, 0, 0 ); }

static inline int _tune_spr( int spr, int action ) {
	return mol_ioctl( MOL_IOCTL_TUNE_SPR, spr, action, 0 ); }

#ifdef __linux__
static inline uint _get_mregs_phys( void ) {
	return mol_ioctl( MOL_IOCTL_GET_MREGS_PHYS, 0, 0, 0 ); }
#endif

struct kernel_vars;
static inline uint _dbg_copy_kvars( int sessid, struct kernel_vars *kv ) {
	return mol_ioctl( MOL_IOCTL_DBG_COPY_KVARS, sessid, (ulong)kv, 0 ); }

#ifdef __darwin__
static inline uint _get_mregs_virt( mac_regs_t **retptr ) {
	return mol_ioctl( MOL_IOCTL_GET_MREGS_VIRT, (int)retptr, 0, 0 ); }

static inline int _call_kernel( void ) {
	return mol_ioctl_simple( MOL_IOCTL_CALL_KERNEL ); }
#endif /* __darwin__ */

#if 0
static inline int _track_dirty_RAM( void ) {
	return mol_ioctl( MOL_IOCTL_TRACK_DIRTY_RAM, 0, 0, 0 ); }

static inline int _get_dirty_RAM( char *retbuf ) {
	return mol_ioctl( MOL_IOCTL_GET_DIRTY_RAM, retbuf, 0, 0 ); }

static inline int _set_dirty_RAM( char *dirtybuf ) {
	return mol_ioctl( MOL_IOCTL_SET_DIRTY_RAM, dirtybuf, 0, 0 ); }
#endif

static inline int _get_irqs( irq_bitfield_t *irqs ) {
	return mol_ioctl( MOL_IOCTL_GET_IRQS, (int)irqs, 0, 0 ); }

static inline int _grab_irq( int irq ) {
	return mol_ioctl( MOL_IOCTL_GRAB_IRQ, irq, 0, 0 ); }

static inline int _release_irq( int irq ) {
	return mol_ioctl( MOL_IOCTL_RELEASE_IRQ, irq, 0, 0 ); }


/* wrappers */
extern mol_kmod_info_t *get_mol_kmod_info( void );

static inline uint _get_pvr( void )		{ return get_mol_kmod_info()->pvr; }
static inline uint _get_tb_frequency( void )	{ return get_mol_kmod_info()->tb_freq; }
static inline uint _get_mol_mod_version( void )	{ return get_mol_kmod_info()->version; }

static inline int _add_mmu_mapping( struct mmu_mapping *m ) {
	return _mmu_map( m, 1 ); }
static inline int _remove_mmu_mapping( struct mmu_mapping *m ) {
	return _mmu_map( m, 0 ); }

static inline int _breakpoint_flags( int flags ) { 
	dbg_op_params_t pb;
	pb.operation = DBG_OP_BREAKPOINT_FLAGS;
	pb.param = flags;
	return _debugger_op( &pb );
}

#endif   /* _H_WRAPPER */
