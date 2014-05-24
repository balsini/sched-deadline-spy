sched-deadline-spy
==================

Kernel module which, for each SCHED_DEADLINE scheduled process, creates a new readable /proc/sched_deadline/[PID], showing tasks' execution information

Requisites
==================

Kernel headers are not enough.
Please, make sure you have the complete kernel source code of your kernel in your /usr/src folder.