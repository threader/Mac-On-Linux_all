/* 
 *   Creation Date: <2004/01/23 11:44:09 samuel>
 *   Time-stamp: <2004/03/13 18:50:30 samuel>
 *   
 *	<dev.c>
 *	
 *	IOCTL device interface
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "archinclude.h"
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <miscfs/devfs/devfs.h>
#include "version.h"
#include "mol-ioctl.h"
#include "kernel_vars.h"
#include "misc.h"
#include "osx.h"
#include "mmu.h"
#include "uaccess.h"
#include "mtable.h"
#include "asmfuncs.h"

static mol_mutex_t 	initmutex;
static int		opencnt;

#define			MINOR_OFFSET	0x10

void
prevent_mod_unload( void )
{
	printk("prevent_mod_unload unimplemented\n");
}

static int
mol_open( dev_t dev, int flags, int devtype, struct proc *p )
{
	int ret=0;
	int dminor = minor(dev);
	uint session = dminor - MINOR_OFFSET;
	
	down_mol( &initmutex );

	if( dminor ) {
		if( session >= MAX_NUM_SESSIONS )
			ret = EINVAL;
		else if( g_sesstab && g_sesstab->kvars[session] )
			ret = EBUSY;
	}

	if( !ret && !opencnt++ ) {
		if( common_init() ) {
			ret = ENOMEM;
			opencnt = 0;
		}
	}
	up_mol( &initmutex );

	/* printk("mol_open - out %d\n", ret ); */
	return ret;
}

static int
mol_close( dev_t dev, int flags, int devtype, struct proc *p )
{
	uint session = minor(dev) - MINOR_OFFSET;

	if( (uint)session < MAX_NUM_SESSIONS ) {
		kernel_vars_t *kv = g_sesstab->kvars[session];
		if( kv ) {
			unmap_mregs( kv );
			destroy_session( session );
		}
	}

	down_mol( &initmutex );
	if( !--opencnt )
		common_cleanup();
	up_mol( &initmutex );

	return 0;
}

static int
get_info( mol_kmod_info_t *user_retinfo, int size ) 
{
	mol_kmod_info_t info;

	memset( &info, 0, sizeof(info) );
	asm volatile("mfpvr %0" : "=r" (info.pvr) : );
	info.version = MOL_VERSION;
#if 0 
	find_physical_rom( &info.rombase, &info.romsize );
	info.tb_freq = HZ * tb_ticks_per_jiffy;
	info.smp_kernel = HAS_SMP;
#else
	info.tb_freq = 0;
	info.smp_kernel = 0;
	info.rombase = 0;
	info.romsize = 0;
#endif
	if( (uint)size > sizeof(info) )
		size = sizeof(info);
	
	if( copyout(&info, user_retinfo, size) )
		return EFAULT;
	return 0;
}


/************************************************************************/
/*	ioctl								*/
/************************************************************************/

static int
debugger_op( kernel_vars_t *kv, dbg_op_params_t *upb )
{
	dbg_op_params_t pb;
	int ret;

	if( copy_from_user_mol(&pb, upb, sizeof(pb)) )
		return -EFAULT;

	switch( pb.operation ) {
	case DBG_OP_GET_PHYS_PAGE:
#if 0
		ret = dbg_get_linux_page( pb.ea, &pb.ret.page );
#else
		printk("DBG_OP_GET_PHYS_PAGE unimplemented\n");
		ret = -EINVAL;
#endif
		break;
	default:
		ret = do_debugger_op( kv, &pb );
		break;
	}

	if( copy_to_user_mol(upb, &pb, sizeof(pb)) )
		return -EFAULT;
	return ret;
}

static int
arch_handle_ioctl( kernel_vars_t *kv, int cmd, int p1, int p2, int p3 )
{
	int ret;
	
	switch( cmd ) {
	case MOL_IOCTL_GET_DIRTY_FBLINES:  /* short *retbuf, int size -- npairs */
		ret = get_dirty_fb_lines( kv, (short*)p1, p2 );
		break;

	case MOL_IOCTL_DEBUGGER_OP:
		ret = debugger_op( kv, (dbg_op_params_t*)p1 );
		break;

	case MOL_IOCTL_COPY_LAST_ROMPAGE: /* p1 = dest */
	case MOL_IOCTL_GET_MREGS_PHYS:
		return -EINVAL;

	case MOL_IOCTL_SET_RAM: /* void ( char *lvbase, size_t size ) */
		ret = 0;
		kv->mmu.userspace_ram_base = p1;
		kv->mmu.ram_size = p2;
		mtable_tune_alloc_limit( kv, p2/(1024*1024) );
		break;

	case MOL_IOCTL_GET_MREGS_VIRT: /* p1 = mac_regs_t **retptr */
		ret = kv->mregs_virtual ? 0 : -EINVAL;
		if( copy_to_user_mol((mac_regs_t**)p1, &kv->mregs_virtual, sizeof(mac_regs_t*)) )
			ret = -EFAULT;
		break;

	default:
		ret = handle_ioctl( kv, cmd, p1, p2, p3 );
		break;
	}
	return ret;
}


static int
mol_ioctl( dev_t dev, u_long cmd, caddr_t data, int fflag, struct proc *p )
{
	uint session = minor(dev) - MINOR_OFFSET;
	mol_ioctl_pb_t *pb = (mol_ioctl_pb_t*)data;
	kernel_vars_t *kv;
	int ret;
	
	/* printk("mol-ioctl: %ld %x %x %x\n", cmd, pb->arg1, pb->arg2, pb->arg3 ); */

	/* fast path (the only call that does not pass a mol_ioctl_pb parameter) */
	if( cmd == MOL_IOCTL_SMP_SEND_IPI ) {
		/* XXX unimplemented */
		return 0;
	}
	if( cmd == MOL_IOCTL_CALL_KERNEL ) {
		int (*kfunc)( kernel_vars_t *kv, ulong arg1, ulong arg2, ulong arg3 );
		if( session >= MAX_NUM_SESSIONS || !(kv=g_sesstab->kvars[session]) )
			return EINVAL;
		/* printk("call-kernel (%08x): %08x %08x %08x\n", (uint)kv->kcall_routine, kv->kcall_args[0],
		   kv->kcall_args[1], kv->kcall_args[2] ); */
		kfunc = (void*)kv->kcall_routine;
		kv->kcall_routine = NULL;
		if( kfunc ) {
			kv->mregs.rvec_vector = (*kfunc)( kv, kv->kcall_args[0], kv->kcall_args[1], kv->kcall_args[2] );
			return 0;
		}
		return EINVAL;
	}

	switch( cmd ) {
	case MOL_IOCTL_GET_INFO:
		pb->ret = 0;
		return get_info( (mol_kmod_info_t*)pb->arg1, pb->arg2 );

	case MOL_IOCTL_CREATE_SESSION:
		if( session >= MAX_NUM_SESSIONS )
			return EINVAL;
		if( !(fflag & FWRITE) )
			return EPERM;
		
		down_mol( &initmutex );
		if( !(pb->ret=initialize_session(session)) ) {
			kv = g_sesstab->kvars[session];
			init_MUTEX_mol( &kv->ioctl_sem );
			kv->mregs_virtual = map_mregs(kv);
		}
		up_mol( &initmutex );
		break;

	case MOL_IOCTL_DBG_COPY_KVARS:
		session = pb->arg1;
		ret = EINVAL;
		down_mol( &initmutex );
		if( session < MAX_NUM_SESSIONS && (kv=g_sesstab->kvars[session]) )
			ret = copy_to_user_mol( (char*)pb->arg2, kv, sizeof(*kv) );
		up_mol( &initmutex );
		return ret;
		
	default:
		if( session >= MAX_NUM_SESSIONS )
			return EINVAL;
		if( !(kv=g_sesstab->kvars[session]) )
			return EINVAL;

		down_mol( &kv->ioctl_sem );
		ret = arch_handle_ioctl( kv, cmd, pb->arg1, pb->arg2, pb->arg3 );
		up_mol( &kv->ioctl_sem );

		/* non-errors should be return through the parameter block */
		if( ret >= 0 ) {
			pb->ret = ret;
			return 0;
		} else {
			pb->ret = 0;
			return -ret;
		}
	}
	return 0;
}

static struct cdevsw mol_cdevsw = {
	.d_open		= mol_open,
	.d_close	= mol_close,
	.d_read		= eno_rdwrt,
	.d_write	= eno_rdwrt,
	.d_ioctl	= mol_ioctl,
	.d_stop		= eno_stop,
	.d_reset	= eno_reset,
	.d_ttys		= NULL,
	.d_select	= eno_select,
	.d_mmap		= eno_mmap,
	.d_strategy	= eno_strat,
	.d_getc		= eno_getc,
	.d_putc		= eno_putc,
	.d_type		= 0,
};

static int	major = -1;
static void	*devfs_handles[MAX_NUM_SESSIONS+1];

int
dev_register( void )
{
	int i;

	printk("MOL %s kernel module loaded\n", MOL_RELEASE );
	major = cdevsw_add( -1, &mol_cdevsw );
	if( major < 0 )
		return -1;

	init_MUTEX_mol( &initmutex );

	if( !(devfs_handles[0]=devfs_make_node(makedev(major, 0), DEVFS_CHAR, 0, 0, 00700, "mol")) )
		goto error;

	for( i=0; i<MAX_NUM_SESSIONS; i++ ) {
		dev_t dev = makedev( major, i+MINOR_OFFSET );
		if( !(devfs_handles[i+1]=devfs_make_node(dev, DEVFS_CHAR, 0, 0, 00700, "mol%d", i)) )
			goto error;
	}
	return 0;

 error:
	dev_unregister();
	return -1;
}

int
dev_unregister( void )
{
	int i;

	if( major < 0 )
		return 0;

	for( i=0; i<= MAX_NUM_SESSIONS; i++ )
		if( devfs_handles[i] ) {
			devfs_remove( devfs_handles[i] );
			devfs_handles[i] = 0;
		}
	
	printk("Unregistering MOL kernel module\n");
	(void) cdevsw_remove( major, &mol_cdevsw );

	major = -1;
	free_MUTEX_mol( &initmutex );

	/* no more allocations... */
	memory_allocator_cleanup();
	
	return 0;
}
