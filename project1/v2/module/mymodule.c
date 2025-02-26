#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#define PROCFS_NAME "psvis_tree"
// Meta Information
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ahmet Emir Benlice");
MODULE_DESCRIPTION("A module that knows how to greet");

static char input_pid[16] = "1"; // default to PID to 1
static struct proc_dir_entry *proc_file;

void write_process_tree(struct task_struct *task, struct seq_file *m) {
    struct task_struct *child;
    struct list_head *list;

    seq_printf(m, "\"%d\\n%s\";\n", task->pid, task->comm);

    list_for_each(list, &task->children) {
        child = list_entry(list, struct task_struct, sibling);
        seq_printf(m, "\"%d\\n%s\" -> \"%d\\n%s\";\n",
                   task->pid, task->comm, child->pid, child->comm);
        write_process_tree(child, m);
    }
}
static ssize_t psvis_write(struct file *file, const char __user *buffer, size_t count, loff_t *pos) {
    char buf[16];

    if (count >= sizeof(buf)) {
        printk(KERN_ERR "psvis_write: Input too large\n");
        return -EINVAL;
    }

    if (copy_from_user(buf, buffer, count)) {
        printk(KERN_ERR "psvis_write: Error copying data from user space\n");
        return -EFAULT;
    }

    buf[count] = '\0';
    strncpy(input_pid, buf, sizeof(input_pid) - 1);
    input_pid[sizeof(input_pid) - 1] = '\0';

    printk(KERN_INFO "psvis_write: Received PID %s\n", input_pid);
    return count;
}

static int psvis_show(struct seq_file *m, void *v) {
    struct pid *pid_struct;
    struct task_struct *task;

    int pid = simple_strtol(input_pid, NULL, 10);
    pid_struct = find_get_pid(pid);
    if (!pid_struct) {
        seq_printf(m, "PID %d not found.\n", pid);
        return -EINVAL;
    }

    task = pid_task(pid_struct, PIDTYPE_PID);
    if (!task) {
        seq_printf(m, "PID %d not found.\n", pid);
        return -EINVAL;
    }

    seq_puts(m, "digraph ProcessTree {\n");
    write_process_tree(task, m);
    seq_puts(m, "}\n");

    return 0;
}
static int psvis_open(struct inode *inode, struct file *file) {
    return single_open(file, psvis_show, NULL);
}

static const struct proc_ops proc_file_ops = {
    .proc_open = psvis_open,
    .proc_read = seq_read,
    .proc_write = psvis_write,
    .proc_lseek = seq_lseek,
    .proc_release = single_release,
};
static int __init psvis_init(void) {
    proc_file = proc_create(PROCFS_NAME, 0666, NULL, &proc_file_ops);
    if (!proc_file) {
        return -ENOMEM;
    }
    printk(KERN_INFO "psvis module loaded.\n");
    return 0;
}

static void __exit psvis_exit(void) {
    proc_remove(proc_file);
    printk(KERN_INFO "psvis module unloaded.\n");
}

module_init(psvis_init);
module_exit(psvis_exit);
