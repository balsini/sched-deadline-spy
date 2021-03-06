#ifdef DEBUGGING
#define pid_t u64
#endif
#ifdef DEBUGGING_KERNEL_FLAG
#endif

#define PERIODS_TO_REMEMBER 50

#include <linux/slab.h>

/*
 * A circular buffer.
 * It contains:
 *   C: the execution time of the job
 *   time: the time at which the sample
 *     has been taken
 *   miss: represents if the last job resulted
 *     in a deadline miss
 */

struct circular_buffer_t {
  int head, tail, count;
  u64 last;
  u64 C[PERIODS_TO_REMEMBER];
  struct timespec time[PERIODS_TO_REMEMBER];
  char miss[PERIODS_TO_REMEMBER];
};

/*
 * Element of the list which will contain all the
 * data associated to SCHED_DEADLINE tasks.
 * It contains:
 *   next: the next element of the list;
 *   pid: the PID;
 *   dl_descriptor: the pointer to the task_struct.dl
 *     field
 *   task_struct_pointer: the pointer to task_struct
 *   file: the pointer to the file in which statistics
 *     will be pushed
 *   file_ops: the options of the file (needed at
 *     creation time)
 *   buffer: the circular buffer
 */
struct task_list_elem_t {
  struct task_list_elem_t * next;

  pid_t pid;

  void * dl_descriptor;
  struct task_struct * task_struct_pointer;
  struct proc_dir_entry * file;
  struct file_operations * file_ops;

  struct circular_buffer_t buffer;
};

/*
 * Creates a new task_list_elem and inserts it
 * in list's head.
 */
static struct task_list_elem_t * create_task_list_elem(void)
{
  struct task_list_elem_t * t;

  t = kmalloc(sizeof(struct task_list_elem_t), GFP_KERNEL);
  if (t) {
    t->dl_descriptor = NULL;
    t->task_struct_pointer = NULL;
    t->file = NULL;
    t->file_ops = NULL;

    t->buffer.head = 0;
    t->buffer.tail = 0;
    t->buffer.count = 0;

    t->buffer.last = 0;

    t->pid = 0;
    t->next = 0;
  }

  return t;
}

/*
 * Creates a new task_list_elem and inserts it
 * in list's head.
 */
static void insert_task_list_elem(struct task_list_elem_t ** head, struct task_list_elem_t * elem)
{
  elem->next = *head;
  *head = elem;
}

/*
 * Removes all the elements from the list
 */
static void clear_task_list(struct task_list_elem_t ** head)
{
  struct task_list_elem_t * p;

  while (*head) {
    if ((*head)->file != NULL)
      proc_remove((*head)->file);
    if ((*head)->file_ops != NULL)
      kfree((*head)->file_ops);
    p = (*head)->next;
    kfree(*head);
    *head = p;
  }
}
/*
static void print_statistics(struct task_list_elem_t * p)
{
  unsigned int i;
  unsigned int hh;

  while (p) {
    printk(CONSOLE_LOG_LEVEL MODULE_NAME_PRINTK"Task ( %d ):\n", p->pid);
    hh = p->buffer.head;
    for (i=0; i<PERIODS_TO_REMEMBER; i++) {
      printk(CONSOLE_LOG_LEVEL "  %d : %lld\n", i, p->buffer.C[hh]);
      hh = (hh + 1) % PERIODS_TO_REMEMBER;
    }
    p = p->next;
  }
}
*/
/*
 * Returns the pointer of the struct corresponding
 * to the PID parameter received
 */
static struct task_list_elem_t * get_elem_by_task_pid(struct task_list_elem_t * head, pid_t pid)
{
  struct task_list_elem_t * p = head;

  while (p && p->pid != pid)
    p = p->next;

  return p;
}

/*
 * Returns the pointer of the struct corresponding
 * to the dl_descriptor parameter received
 */
static struct task_list_elem_t * get_elem_by_task_dl_descriptor(struct task_list_elem_t * head, void * dl_descriptor)
{
  struct task_list_elem_t * p = head;

  while (p && p->dl_descriptor != dl_descriptor)
    p = p->next;

  return p;
}

/*
 * Returns the pointer of the struct corresponding
 * to the file parameter received
 */
/*
static struct task_list_elem_t * get_elem_by_task_file(struct task_list_elem_t * head, struct proc_dir_entry * file)
{
  struct task_list_elem_t * p = head;

  while (p && p->file != file)
    p = p->next;

  return p;
}
*/

/*
 * Removes the head element from the circular buffer,
 * returning it
 */
static u64 cb_dequeue(struct circular_buffer_t * buffer)
{
  u64 ret;

  if (buffer->count == 0) {
    return -1;
  }
  ret = buffer->C[buffer->head];
  buffer->head = (buffer->head + 1) % PERIODS_TO_REMEMBER;
  buffer->count--;

  return ret;
}

/*
 * Inserts one element in the circular buffer.
 * If the buffer is full, the head element (the oldest) is replaced.
 */
static void cb_enqueue(struct circular_buffer_t * buffer, u64 v, char miss, struct timespec time)
{
  if (buffer->count == PERIODS_TO_REMEMBER)
    cb_dequeue(buffer);
  buffer->C[buffer->tail] = v;
  buffer->miss[buffer->tail] = miss;
  buffer->time[buffer->tail] = time;
  buffer->tail = (buffer->tail + 1) % PERIODS_TO_REMEMBER;
  buffer->count++;
}

/*
static u64 cb_get(struct circular_buffer_t * buffer, int i)
{
  return buffer->C[(buffer->head + i) % PERIODS_TO_REMEMBER];
}
*/

/*
 * Returns the string length
 */
int kstrlen(char * s)
{
  int i=0;
  while (s[i] != '\0') i++;
  return i;
}

/*
 * Modifies an array of chars, reverting its order
 */
char * kstrrev(char *str) {
  char *p1, *p2;
  if (!str || !*str)
    return str;
  for (p1 = str, p2 = str + kstrlen(str) - 1; p2 > p1; ++p1, --p2) {
    *p1 ^= *p2;
    *p2 ^= *p1;
    *p1 ^= *p2;
  }
  return str;
}

/*
 * Transforms an integer into an array of chars in a
 * certain base
 */
void kitoa(int n, char *s, int b) {
  static char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
  int i=0, sign;

  if ((sign = n) < 0)
    n = -n;
  do {
    s[i++] = digits[n % b];
  } while ((n /= b) > 0);
  if (sign < 0)
    s[i++] = '-';
  s[i] = '\0';
  s = kstrrev(s);
}

/*
 * String concatenation
 */
void kstrcat(char * dst, char * src)
{
  int i = 0;
  int j = 0;
  while (dst[i] != '\0')
    i++;
  do {
    dst[i] = src[j];
    i++;
  } while (src[j++] != '\0');
}
