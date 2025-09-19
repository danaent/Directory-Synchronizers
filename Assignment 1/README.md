# Real-time Directory Synchronizer

This is a directory synchronization tool that monitors a list of source and target directory pairs and ensures that the target directory remains identical to the source directory. The program only works with flat directories, i.e. directories that only contain regular files.

Directories are monitored using the inotify library. All changes to the source directories (file creation, deletion and modification), are immediately replicated to the target directories. Each change is assigned as a task to different worker process that is created using ```fork()``` and ```exec()```. This allows for multiple synchronization jobs to be running independently of each other and of the main program. The project also includes a command-line interface for adding new source-target pairs, canceling the monitoring of existing pairs and checking the status of monitored pairs.

## Compilation

Running ```make all``` creates three executable files: ```fss_manager```, ```fss_console``` and ```worker```. These are all necessary to run the project.

## Usage

To begin, run the following. Make sure both ```fss_manager``` and ```worker``` have been compiled.

```
./fss_manager -c <config_file> -l <manager_logfile> -n worker_limit
```

- ```<config_file>``` is a file that contains pairs of directories. The file should have the form:

```
(source_dir1, target_dir1)
(source_dir2, target_dir2)
(source_dir3, target_dir3)
.
.
.
```

These are the pairs of directories that will be monitored and synchronized.

- ```<manager_logfile>``` is the file where ```fss_manager```'s log messages will be written.
- ```<worker_limit>``` is an optional flag. It is the maximum number of worker processes that can be running at the same time. If not set, the default is 5.


Now all directory pairs should be identical. Every change in a source directory should be mirrored to the target directory.

For the command-line interface, at a different terminal run:

```
./fss_console -l <console_logfile>
```

- ```<console_logfile>``` is the file where fss_console's log messages will be written. It should be different from ```<manager_logfile>```.


Now you can input any command from the ones below. Each command is executed only after the previous one has fully finished.

## Commands

```
add <source_dir> <target_dir>
```

Synchronization and monitoring is initiated for the given pair of source and target directories.

```
cancel <source_dir>
```

Monitoring is stopped for the given source directory. Any further changes will not be replicated to its target directory. Info on the directory remains stored, only its status is set as 'inactive'.

```
sync <source_dir>
```

Begins a full synchronization of ```<source_dir>``` to its target directory. If this directory has been canceled and set to 'inactive', it becomes 'active' and is monitored again, remaining linked to its initial target directory.

```
status <source_dir>
```

Displays information about a source directory. It displays:

- The directory name (Directory).
- Its target directory (Target).
- Time and date of last synchronization (Last Sync).
- Number of errors that have occured, such as inability to open a file (Errors).
- Active or inactive status (Status).

```
shutdown
```

Terminates both ```fss_manager``` and ```fss_console``` after all active worker processes have finished their jobs.

## Log messages

### fss_manager log

Messages like the following could be printed in the fss_manager logfile after a successful run of the program.

```
[2025-02-10 10:00:01] Added directory: /home/user/docs -> /backup/docs
```

Source and target pair (/home/user/docs, /backup/docs) has been added, either from the config file or the ```add``` command.

```
[2025-02-10 10:00:01] Monitoring started for /home/user/docs
```

Printed right after ```Added directory``` message to show that the new source directory is being monitored for changes.

```
[2025-02-10 10:23:01] Monitoring stopped for /home/user/docs
```

Printed after a ```cancel /home/user/docs``` command, if the requested directory exists.

```
[2025-02-10 10:23:01] Syncing directory: /home/user/docs -> /backup/docs
```

Printed after a ```sync /home/user/docs```, if the requested directory exists.

```
[TIMESTAMP] [SOURCE_DIR] [TARGET_DIR] [WORKER_PID] [OPERATION] [RESULT] [DETAILS]
```

After any synchronization job that occurs either from a ```sync``` command or an inotify alert, a message like the above is printed.

- ```TIMESTAMP``` is the time and date the job finished.
- ``` SOURCE_DIR``` is the source directory.
- ```TARGET_DIR``` is the target directory.
- ```WORKER_PID``` is the process id of the worker process that completed the job.
- ```OPERATION``` can be ```FULL```, ```ADDED```, ```MODIFIED```, ```DELETED```.
- ```RESULT``` can be ```SUCCESS```, ```ERROR```, ```PARTIAL```.
- ```DETAILS``` are more details on the result.

A final log file may look like this.

```
[2025-19-09 12:40:11] Added directory: /home/user/docs -> /backup/docs
[2025-19-09 12:40:11] Monitoring started for /home/user/docs
[2025-19-09 12:40:11] [/home/user/docs] [/backup/docs] [8197] [FULL] [SUCCESS] [21 files copied]
[2025-19-09 12:40:47] [/home/user/docs] [/backup/docs] [8305] [ADDED] [SUCCESS] [File: somefile.txt]
[2025-19-09 12:40:47] [/home/user/docs] [/backup/docs] [8307] [MODIFIED] [SUCCESS] [File: somefile.txt]
[2025-19-09 12:41:48] [/home/user/docs] [/backup/docs] [8357] [DELETED] [SUCCESS] [File: somefile.txt]
[2025-19-09 12:42:11] Monitoring stopped for /home/user/docs
[2025-19-09 12:42:17] Syncing directory: /home/user/docs -> /backup/docs
[2025-19-09 12:42:17] [/home/user/docs] [/backup/docs] [8433] [FULL] [SUCCESS] [21 files copied]
```

Some of these messages are also printed on the terminal, along with more messages that indicate different errors.

### fss_console log

The console log file only contains the commands that were executed. For example:

```
[2025-02-10 10:00:01] Command add /home/user/docs -> /backup/docs
[2025-02-10 10:00:01] Command status /home/user/docs
[2025-02-10 10:23:01] Command cancel /home/user/docs
[2025-02-10 10:23:01] Command sync /home/user/docs
[2025-02-10 10:23:01] Command shutdown /home/user/docs
```