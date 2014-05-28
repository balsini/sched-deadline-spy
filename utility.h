#ifdef DEBUGGING
#define pid_t u64
#endif
#ifdef DEBUGGING_KERNEL_FLAG
#endif

#define PERIODS_TO_REMEMBER 500

struct complex_ret_t {
  int value;
  void * pointer;
};

struct circular_buffer_t {
  int head, tail, count;
  u64 last;
  u64 C[PERIODS_TO_REMEMBER];
  struct timespec time[PERIODS_TO_REMEMBER];
  char miss[PERIODS_TO_REMEMBER];
};

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

int kstrlen(char * s)
{
  int i=0;
  while (s[i] != '\0') i++;
  return i;
}

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
