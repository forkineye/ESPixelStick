# ESPixelStick Source Structure

All code aside from the main sketch itself should reside here. The root of this directory is for core parts of the ESPixelStick code and / or pieces that should not reside in the following directories:

- ```input``` - Show input modules should reside here and inherit from ```c_InputCommon``` as defined in ```InputCommon.hpp```.
- ```ouput``` - Show output modules should reside here and inherit from ```c_OutputCommon``` as defined in ```OutputCommon.hpp```.
- ```service``` - Auxiliary services should reside here. These include non-show critical input mechansims like FPP discovery / configuration mechanisms. 

See InputMgr and OutputMgr for adding modules to the system. 