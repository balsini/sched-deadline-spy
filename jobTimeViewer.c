#include <linux/module.h>
#include <linux/kprobes.h>

#include <linux/kernel.h>

#include <linux/pid.h>
#include <linux/sched.h>

#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/spinlock.h>

#include <kernel/sched/sched.h>
#include <fs/proc/internal.h>

#define CONSOLE_LOG_LEVEL KERN_EMERG

#define MODULE_NAME "jobTimeViewer"
#define MODULE_NAME_PRINTK "jobTimeViewer : "
#define SCHED_DL_PROC_FILENAME "deadline_stat"

#define PROC_FS_FOLDER_NAME "sched_deadline"

#include "utility.h"

struct task_list_elem_t * head;

static struct proc_dir_entry * sched_dl_folder;

void print_file_from_old_to_new(struct seq_file * m)
{
  struct task_list_elem_t * p;
  unsigned int i, count;

  p = (struct task_list_elem_t *)(m->private);
  count = p->buffer.count;
  i = p->buffer.head;

  // Printing from the oldest to the newest
  while (count > 0) {
    seq_printf(m, "%lu %ld %lld ; \n",
               (unsigned long)(p->buffer.time[i].tv_sec),
               (long int)p->buffer.time[i].tv_nsec,
               p->buffer.C[i]);
    i = (i + 1) % PERIODS_TO_REMEMBER;
    --count;
  }
}

void print_file_from_new_to_old(struct seq_file * m)
{
  struct task_list_elem_t * p;
  unsigned int i, count;

  p = (struct task_list_elem_t *)(m->private);
  count = p->buffer.count;
  i = p->buffer.tail - 1;

  // Printing from the oldest to the newest
  while (count > 0) {
    if (i < 0)
      i = PERIODS_TO_REMEMBER - 1;
    seq_printf(m, "%lu %ld %lld ; \n",
               (unsigned long)(p->buffer.time[i].tv_sec),
               (long int)p->buffer.time[i].tv_nsec,
               p->buffer.C[i]);
    --i;
    --count;
  }
}

void print_file_hud(struct seq_file * m)
{
  struct task_list_elem_t * p;
  unsigned int count;

  p = (struct task_list_elem_t *)(m->private);
  count = p->buffer.count;

  seq_printf(m, "pid ( %d ) data# ( %d )\n", p->pid, count);
  seq_printf(m, "-----------------------\n");

}

static int sched_dl_show(struct seq_file * m, void * v)
{
  //print_file_hud(m);
  //print_file_from_old_to_new(m);
  print_file_from_new_to_old(m);
  return 0;
}

static int sched_dl_open(struct inode * inode, struct  file * file)
{
  int ret;

  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"A created file with inode: %p\n", inode);
  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"A created file with address: %p\n", file);
  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"file->private: %p\n", PDE_DATA(inode));

  ret = single_open(file, sched_dl_show, PDE_DATA(inode));

  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"B created file with address: %p\n", file);

  return ret;
}

/*
 * A replenishment has been requested
 */
static void inst_replenish_dl_entity(struct sched_dl_entity *dl_se,
                                     struct sched_dl_entity *pi_se)
{

  struct task_list_elem_t * p;
  u64 exec_time;
  u64 server_bandwidth_used;
  char miss;

  p = get_elem_by_task_dl_descriptor(head, (void *)dl_se);
  if (p) {
    exec_time = p->task_struct_pointer->se.sum_exec_runtime - p->buffer.last;
    miss = exec_time >= dl_se->dl_deadline ? 'Y' : 'N';
    server_bandwidth_used = dl_se->dl_deadline / exec_time;

    //These two values are always the same!!
    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last dl_se->runtime: %lld\n", dl_se->runtime);
    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last pi_se->runtime: %lld\n", pi_se->runtime);

    // Relative deadline
    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last dl_se->dl_deadline: %lld\n", dl_se->dl_deadline);
    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last pi_se->dl_deadline: %lld\n", pi_se->dl_deadline);

    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last dl_se->dl_period: %lld\n", dl_se->dl_period);
    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last pi_se->dl_period: %lld\n", pi_se->dl_period);

    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last exec_time: %lld\n", exec_time);

    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last server_bandwidth_used: %lld\n", server_bandwidth_used);

    cb_enqueue(&(p->buffer),
               exec_time,
               miss,
               current_kernel_time());

    //cb_enqueue(&(p->buffer),
    //           pi_se->dl_runtime - dl_se->runtime,
    //           miss,
    //           current_kernel_time());

    //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"last exec: %lld, miss: %c\n\n", exec_time, miss);

    p->buffer.last = p->task_struct_pointer->se.sum_exec_runtime;
  }

  jprobe_return();
}

static void inst_enqueue_task_dl(struct rq * rq,
                                 struct task_struct * ts,
                                 int flags)
{
  struct task_list_elem_t * p;
  char path_buffer[50];

  path_buffer[0] = '\0';

  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"replenishing PID( %d ) PGRP( %d )\n", ts->pid, ts->tgid);

  p = get_elem_by_task_pid(head, ts->pid);
    if (!p) {
      //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"unknown pid found, creating instance\n");
      p = create_task_list_elem();
      if (p) {
        insert_task_list_elem(&head, p);

        p->pid = ts->pid;
        p->dl_descriptor = &(ts->dl);
        p->task_struct_pointer = ts;

        if (sched_dl_folder) {
          // creating process file //

          p->file_ops = kmalloc(sizeof(struct file_operations), GFP_KERNEL);
          if (p->file_ops) {
            p->file_ops->owner = THIS_MODULE;
            p->file_ops->open = sched_dl_open;
            p->file_ops->read = seq_read;
            p->file_ops->llseek = seq_lseek;
            p->file_ops->release = single_release;

            kitoa(p->pid, path_buffer, 10);
            p->file = proc_create_data(path_buffer, 0444, sched_dl_folder, p->file_ops, p);

            //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"created file with name: %s\n", path_buffer);
            //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"created file with address: %p\n", p->file);
            //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"passed parameter: %p\n", p);
          }
        }
      }
    }

  //printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"enqueuing new value\n");

  jprobe_return();
}

static struct jprobe enqueue_task_dl_jprobe = {
  .kp = {
    .symbol_name	= "enqueue_task_dl",
  },
  .entry = (kprobe_opcode_t *) inst_enqueue_task_dl
};

static struct jprobe replenish_dl_entity_jprobe = {
  .kp = {
    .symbol_name	= "update_dl_entity",
    //.symbol_name	= "replenish_dl_entity",
  },
  .entry = (kprobe_opcode_t *) inst_replenish_dl_entity
};

int init_module(void)
{
  head = 0;

  sched_dl_folder = proc_mkdir(PROC_FS_FOLDER_NAME, NULL);
  if(!sched_dl_folder) {
    printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"error creating /proc/ folder\n");
    return -ENOMEM;
  }

  register_jprobe(&enqueue_task_dl_jprobe);

  printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"plant jprobe at %p, handler addr %p\n",
         enqueue_task_dl_jprobe.kp.addr, enqueue_task_dl_jprobe.entry);

  register_jprobe(&replenish_dl_entity_jprobe);

  printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"plant jprobe at %p, handler addr %p\n",
         enqueue_task_dl_jprobe.kp.addr, enqueue_task_dl_jprobe.entry);

  return 0;
}

void cleanup_module(void)
{
  unregister_jprobe(&replenish_dl_entity_jprobe);
  unregister_jprobe(&enqueue_task_dl_jprobe);

  printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"jprobe unregistered\n");

  //print_statistics(head);
  clear_task_list(&head);

  remove_proc_entry(PROC_FS_FOLDER_NAME, NULL);
}

MODULE_AUTHOR("Alessio Balsini");
MODULE_DESCRIPTION("For each SCHED_DEADLINE scheduled process, a new readable /proc/sched_deadline/[PID] inode will be created, containing tasks' execution information");
MODULE_LICENSE("GPL");
