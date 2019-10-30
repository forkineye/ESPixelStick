# ESPixelStick Source Structure

***This is a work in progress and is not yet fully implemented***

All code aside from the main sketch itself should reside here. The root of this directory is for core parts of the ESPixelStick code and / or pieces that should not reside in the following directories:

- ```input``` - Show input modules should reside here and inherit from ```_Input``` as defined in ```_Input.h```.
- ```ouput``` - Show output modules should reside here and inherit from ```_Output``` as defined in ```_Output.h```.
- ```service``` - Auxiliary services should reside here. These include non-show critical input mechansims like MQTT and discovery / configuration mechanisms. ***not yet implemented***

To implement a module, add an entry to the appropiate map under the ```Module Maps``` section of the main sketch.  The entry must include a unique key string and an instantiation of the appropiate derived class.  Keep module memory allocation light as possible upon construction and allocate / de-allocate as needed in the ```init()``` and ```destroy()``` member functions.  ***Aside from the Module Map in the main sketch, no other code or modules should be modified to support a specific module.***
