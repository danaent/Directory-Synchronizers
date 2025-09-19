# Remote Directory Synchronizer

This is a directory synchronization tool that allows for copying of the contents of remote source directories to remote target directories. It works with flat directories, i.e. directories that only contain regular files. 

The project contains three programs: ```nfs_client```, ```nfs_manager``` and ```nfs_console```. Despite its name, ```nfs_client``` is a server program that runs on each remote device, listening for incoming connections. ```nfs_manager```  communicates with the different ```nfs_client``` programs, sending LIST requests to get the file list from the source directory, PULL requests to fetch file contents, and PUSH requests to write the data to the target directory. It is a multi-threaded program, meaning that multiple synchronization jobs can be running concurrently. Finally, ```nfs_console``` is a command-line interface for adding new source-target pairs and canceling already running synchronization jobs.

## Compilation

Running ```make all``` creates three executable files: ```nfs_manager```, ```nfs_console``` and ```nfs_client```. You can also run ```make <program_name>``` to only create the program that you need.

## Usage

On each device with a source or target directory, run the following:

```
./nfs_client -p <port_number>
```

```<port_number>``` is the port that ```nfs_client``` will use to listen for incoming commands. To shut down an ```nfs_client```, use ```ctrl+c``` or any other exit method.

On your own device, run:

```
./nfs_manager -c <config_file> -p <port_number> -l <manager_logfile> -b <buffer_size> -n <worker_limit>
```

- ```<config_file>``` is a file that contains pairs of directories. The file should have the form:

```
(/source1@host1:port1, /target2@host2:port2)
(/source3@host3:port3, /target4@host4:port4)
(/source5@host5:port5, /target6@host6:port6)
.
.
.
```

In an entry of the form ```(/source1@host1:port1, /target2@host2:port2)```:

1. ```source1``` and ```target1``` are the names of the directories. The paths are relative to the directory where the respective ```nfs_client``` is located. For example, if an ```nfs_client``` is located inside the directory ```/home/user/mydir```, then ```/source1``` means ```/home/user/mydir/source1```.

2. ```host1``` and ```host2``` are the devices where each directory is located, along with its ```nfs_client```. They can be IP addresses or host names.

3. ```port1``` and ```port2``` are the ports that each directory's ```nfs_client``` uses to listen for incoming commands. It is the same port that is passed to the ```nfs_client``` program with the ```-p``` flag.

- ```<port_number>``` is the port that ```nfs_manager``` will use to listen for incoming commands from ```nfs_console```, meaning that these two programs can be remote as well.
- ```<manager_logfile>``` is the file where ```nfs_manager```'s log messages will be written.
- ```<buffer_size>``` is the size of a cyclical buffer where synchronization jobs will be placed and then removed and executed by worker threads. Each synchronization job involves the copying of one file from the source to the target directory, NOT the copying of the entire directory.
- ```<worker_limit>``` is an optional flag. It is the maximum number of worker threads that can be running at the same time. If not set or a <= 0 value is given, the default is 5.


After running ```nfs_manager```, all directory pairs should be in the process of being synchronized. You may now run on a different terminal and/or device:

```
./nfs_console -h <host> -p <host_port> -l <console_logfile>
```

- ```<host>``` is the device that ```nfs_manager``` is running on. It can be an IP address or a host name.
- ```<host_port>``` is the port that ```<nfs_manager>``` uses to listen for commands. It is the same port that is passed to ```nfs_manager``` with its own ```-p``` flag.
- ```<console_logfile>``` is the file where nfs_console's log messages will be written. It should be different from ```<manager_logfile>```.

Now you can input any command from the ones below. Each command is executed only after the previous one has fully finished.

## Commands

```
add src_name@src_host:src_port tar_name@tar_port@tar_host
```

Synchronization is initiated for the given pair of directories. All files from the source directory are placed in the buffer as separate jobs and await to be copied.

```
cancel <src_name>
```

Synchronization stops for any files from source directories with the name ```<src_name>```, regardless of host and port number. If there was never a directory with this name, or directories with this name have already finished their synchronization, nothing happens. The directory name should have the format it has on the config file, i.e. start with ```/```.

```
shutdown
```

Terminates both ```nfs_manager``` and ```nfs_console``` after all jobs in the buffer have been completed.

## Log messages

### nfs_manager log

Messages like the following could be printed in the fss_manager logfile after a successful run of the program.

```
[2025-02-10 10:00:01] Added file: /dir1/file1@1.2.3.4:8080 -> /dir2/file1@4.5.6.7:8090
```

This message is printed when the pair ```(/dir1@1.2.3.4:8080, /dir2@4.5.6.7:8090)``` is read from the ```config_file``` or the command ```add /dir1@1.2.3.4:8080 /dir2@4.5.6.7:8090``` is run. It is printed for every separate file inside ```dir1```.

```
[2025-02-10 10:23:01] Synchronization stopped for /dir1@1.2.3.4:8080
```

This message is printed after a ```cancel /dir1``` command. It is printed for every directory named ```/dir1``` that still has files in the buffer queue waiting to be copied.

```
[TIMESTAMP] [SOURCE_DIR] [TARGET_DIR] [THREAD_PID] [OPERATION] [RESULT] [DETAILS]
```

This message is printed after every ```PUSH``` or ```PULL``` command that is sent from ```nfs_manager``` to an ```nfs_client```.

- ```TIMESTAMP``` is the time and date the job finished.
- ```SOURCE_DIR``` is the source directory.
- ```TARGET_DIR``` is the target directory.
- ```THREAD_PID``` is the thread id of the worker thread that completed the job.
- ```OPERATION``` can be ```PUSH``` or ```PULL```.
- ```RESULT``` can be ```SUCCESS``` or ```ERROR```.
- ```DETAILS``` is the number of bytes that were pushed/pulled if ```RESULT``` is ```SUCCESS```, or an error message if ```RESULT``` is ```SUCCESS```. Sometimes that number is 0, because some ```PUSH``` and ```PULL``` commands are used to create a new file or close an existing file.

Some of these messages are also printed on the terminal, along with more messages that indicate different errors.

### nfs_console log

The console log file only contains the commands that were executed. For example:

```
[2025-09-19 16:25:07] Command add /dir1@1.2.3.4:8080 -> /dir2@4.5.6.7:8090
[2025-09-19 16:25:09] Command cancel /dir1
[2025-09-19 16:25:13] Command shutdown
```

