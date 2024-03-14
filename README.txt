1.  Compilation and Building

To compile and build the server program, use the provided Makefile: 
$ make

This command will produce the executable named `assignment3`.



2. Starting the Server

To start the server, use the following command:
$ ./assignment3 -l <port_number> -p "<search_pattern>"

For example, to start the server listening on port 12345 and search for the pattern "happy", you'd use: 

$ ./assignment3 -l 12345 -p "happy"
Note: The port number can be changed as required.



3. Using a Client Program to Send Data

To send data to the server, you can use the `netcat` utility or any client program that supports TCP connections.

Example using `netcat`:

$ nc localhost <port number> -i "delay number" < "Your data here"

-Port Number the same as the server you started
-Delay number is of your own choosing
-Replace "Your data here" with the actual data you want to send. Ensure the server is running and listening on the specified port before sending data. Can be in form of UTF-8 formatted file (.txt)

for example:
nc localhost 12345 -i 10 < book.txt





