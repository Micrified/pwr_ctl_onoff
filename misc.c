static void pwr_ctl_write_pid_to_buffer (pid_t pid)
{
	off_t i = OUT_BUF_SIZE - 1;

	// Reset the buffer
	memset(g_out_buffer, '0', OUT_BUF_SIZE);

	// Write the pid as an UTF8 ASCII string
	do {
		g_out_buffer[i--] += pid % 10;
		pid = pid / 10;
	} while (i >= 0 && pid > 0);
}

static int pwr_set_delegate_task (pid_t pid)
{
	int retval = 0;

	// Ignore PID value 0
	if (pid == 0) {
		return retval;
	}

	// Inform reclaimer that this module is entering critical section
	rcu_read_lock();

	// Get the pid struct associated with the PID
	if ((g_pid_struct_p = find_get_pid(pid)) == NULL) {
		pr_err("find_pid(): failed\n");
		retval = -ESRCH;
		goto unlock;
	}

	// Locate the task struct associated with the PID
	if ((g_task_struct_p = pid_task(g_pid_struct_p, PIDTYPE_PID)) == NULL) {
		pr_err("pid_task(): failed\n");
		retval = -ESRCH;
		goto unlock;
	}

	// Inform reclaimer that this module is exiting critical section
unlock:
	rcu_read_unlock();
	return retval;
}