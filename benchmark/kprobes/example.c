#include <linux/kprobes.h>
#include <linux/module.h>
#include <linux/sched.h>

static struct kprobe probes[] = {
#ifdef CONFIG_X86_64
	{ .symbol_name = "__x64_sys_open" },
	{ .symbol_name = "__x64_sys_close" },
	{ .symbol_name = "__x64_sys_read" },
	{ .symbol_name = "__x64_sys_write" },
	{ .symbol_name = "__x64_sys_stat" },
	{ .symbol_name = "__x64_sys_clone" },
#elif defined(CONFIG_RISCV)
	{ .symbol_name = "__riscv_sys_openat" },
	{ .symbol_name = "__riscv_sys_close" },
	{ .symbol_name = "__riscv_sys_read" },
	{ .symbol_name = "__riscv_sys_write" },
	{ .symbol_name = "__riscv_sys_newfstatat" },
	{ .symbol_name = "__riscv_sys_clone" },
#else
#error "Only x86-64 and RISC-V are supported"
#endif
};
static int target_pid;
module_param(target_pid, int, 0444);
MODULE_PARM_DESC(target_pid, "TGID of the process to intercept");

static int log_syscall(struct kprobe *probe, struct pt_regs *regs)
{
	if (current->tgid != target_pid)
		return 0;

	pr_info("intercepted %s from pid %d\n", probe->symbol_name,
		current->tgid);
	return 0;
}

static int __init example_init(void)
{
	size_t i;
	int ret;

	if (target_pid <= 0)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(probes); i++) {
		probes[i].pre_handler = log_syscall;
		ret = register_kprobe(&probes[i]);
		if (ret) {
			pr_err("failed to register %s: %d\n",
				probes[i].symbol_name, ret);
			goto unregister;
		}
	}
	return 0;

unregister:
	while (i--)
		unregister_kprobe(&probes[i]);
	return ret;
}

static void __exit example_exit(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(probes); i++)
		unregister_kprobe(&probes[i]);
}

module_init(example_init);
module_exit(example_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PID-filtered logging of open, close, read, write, stat, and clone");
