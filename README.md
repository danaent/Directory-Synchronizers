# Directory Synchronizers

This project contains two directory synchronization tools that were developed for the Systems Programming course in UoA's Department of Informatics and Telecommunications, during the Spring Semester of 2024-2025. Both assignments got 100/100.

- Real-time Directory Synchronizer: uses inotify and concurrent worker processes to keep pairs of source-target directories synchronized in real time. 
- Remote Directory Synchronizer: uses a client-server model to synchronize pairs of source-target directories across different devices using PUSH and PULL requests. Uses multi-threading to achieve concurrency. Not real-time.

Both synchronizers include a command-line interface for managing directory pairs.