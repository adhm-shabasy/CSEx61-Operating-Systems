# Lab 5: Custom CPU Scheduler & IPC Simulation

## 1. Objectives
* Understand how an Operating System schedules processes.
* Implement Inter-Process Communication (IPC) using POSIX Message Queues.
* Manage process creation and termination using `fork()`, `exec()`, and `SIGKILL`.
* Control process execution using UNIX Signals (`SIGSTOP` and `SIGCONT`).

## 2. Problem Statement
In this lab, you will build a deterministic simulation of a process scheduler. Unlike a real-time system that relies on `sleep()`, this lab uses **Virtual Time** (Discrete Event Simulation). This ensures that the simulation runs instantly and produces the exact same results every time, regardless of CPU speed.

You must complete a multi-process system consisting of:
1. **The Process Generator (`process_generator.c`):** Reads the input and sends all process data to the Scheduler via a POSIX Message Queue.
2. **The Scheduler (`scheduler.c`):** The "brain" of the OS. It buffers all processes, then runs a virtual clock to execute them according to a chosen algorithm.

## 3. System Architecture & Requirements

### IPC & The Process Generator
* The generator reads `processes.txt`. Format: `[ID] [Arrival Time] [Run Time] [Priority]`.
* You must initialize a **POSIX Message Queue** (using `mq_open`).
* The Generator should send all processes to the queue immediately and then send a **termination message** (a process with `ID = -1`) to signal the end of the input.

### The Scheduler & Virtual Time
* **Initialization:** The Scheduler must read **all** messages from the POSIX Message Queue before starting the virtual clock.
* **The Clock Loop:** Instead of real-time seconds, use an integer `current_time` that increments from 0. 
* **Process Creation:** When `current_time` matches a process’s `arrival_time`, `fork()` and `exec()` the dummy process. Immediately `SIGSTOP` it until it is chosen to run.
* **Execution:** Use `kill(pid, SIGCONT)` to run a process and `kill(pid, SIGSTOP)` to preempt it.
* **Termination:** When a process’s `remaining_time` reaches 0, the Scheduler must terminate it using `kill(pid, SIGKILL)` and clean up the zombie process using `waitpid()`.

### The Scheduling Algorithms
Execute the scheduler using: `./scheduler <algo_id> [quantum]`
1. **FCFS (Algo 1):** Non-Preemptive. Processes run to completion in order of arrival.
2. **Round Robin (Algo 2):** Preemptive. Processes run for a Time Quantum ($Q$). If the quantum expires, the process is preempted and moved to the back of the queue.
3. **HPF (Algo 3):** Non-Preemptive. The process with the highest priority (lowest integer value) runs next. 
    * **Tie-Breaker:** If priorities are equal, use FCFS (Arrival Time).

## 4. Logging & Deterministic Output
Your `scheduler.c` must log every state change to `scheduler.log`. **The `[Time]` must be your virtual clock integer.**

* **Started:** `At time [Time] process [ID] started arr [Arrival] total [RunTime] remain [RemTime] wait [WT]`
* **Stopped:** `At time [Time] process [ID] stopped arr [Arrival] total [RunTime] remain [RemTime] wait [WT]`
* **Resumed:** `At time [Time] process [ID] resumed arr [Arrival] total [RunTime] remain [RemTime] wait [WT]`
* **Finished:** `At time [Time] process [ID] finished arr [Arrival] total [RunTime] remain 0 wait [WT] TA [TAT]`

### Metrics Report
At the end, generate `metrics.txt`:
```text
CPU utilization = [XX.XX]%
Avg TAT = [XX.XX], Avg WT = [XX.XX]
```

## 5. Deliverables
You must submit:
1. `process_generator.c` (With your IPC implementation).
2. `scheduler.c` (With your algorithm and virtual time implementation).

## 6. Notes & Hints
* **Linker Flag:** You must include `-lrt` in your compilation to use POSIX Message Queues.
* **Deterministic Timing:** Since we use virtual time, the simulation should finish almost instantly. 
* **Signal Handling:** Use `usleep(1000)` (1 millisecond) at the end of each virtual clock tick to allow the Linux kernel time to process signals.
* **Cleanup:** Use `mq_unlink` at the end of the scheduler to remove the message queue from the system.

