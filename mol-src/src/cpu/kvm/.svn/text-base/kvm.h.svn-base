extern int kvm_ioctl(int type, ...);
extern int kvm_vm_ioctl(int type, ...);
extern int kvm_vcpu_ioctl(int type, ...);
extern void kvm_regs_kvm2mol();
extern void kvm_regs_mol2kvm();
extern int kvm_init(void);
extern int kvm_trigger_irq(void);
extern int kvm_untrigger_irq(void);
extern void kvm_regs_kvm2mol(void);
extern void kvm_regs_mol2kvm(void);
extern int kvm_set_user_memory_region(struct mmu_mapping *m);
extern int kvm_del_user_memory_region(struct mmu_mapping *m);

extern int kvm_get_dirty_fb_lines(short *rettable, int table_size_in_bytes);
extern int kvm_set_fb_size(int bytes_per_row, int height);

extern struct kvm_run *kvm_run;
