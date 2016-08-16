Assignment 4

Instructions on how the module will be tested :

Copy the files asp_4.c, Makefile and userapp.c to a virtual linux machine
and follow the following steps:

1) change usermode to root using su - command
2) Compile driver module : $ make from the directory you have copied in the virtual machine(We can avoid warnings)

3) Load module : $ insmod asp_4.ko ndevices=<num_devices>

3) Test driver :
	1) Compile userapp : $ make app
	2) Run userapp : $  ./userapp <device_number>			
		where device_number identifies the id number of the device to be tested.   

	Note : userapp has to be executed with root as the user
		   
4) Unload module : $  rmmod asp_4.ko


changes to userapp:-
1> restrict the write size with string length of the data instead of whole buffer size to append data following every write operation.
2> implemented while loop to do multiple changes at one go and you can enter 'e' to exit the program

Code Logic ->
1> get the number of devices we need to create using module_param
2> allocate the memory of driver structure to each and every device and represnt this device as node
3> insert every node in the kernel linked list
4> traverse each node using linked list to get the minor of the device which is requested to open and store it in private data for
   further reference
5> implemented lseek,ioctl,write and read operations as asked in the assignment.
