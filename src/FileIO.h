#pragma once
/*
* FileIO.h - Static methods for common file I/O routines
*
* Project: ESPixelStick - An ESP8266 / ESP32 and E1.31 based pixel driver
* Copyright (c) 2020 Shelby Merrick
* http://www.forkineye.com
*
*  This program is provided free for you to use in any way that you wish,
*  subject to the laws and regulations where you are using it.  Due diligence
*  is strongly suggested before using this code.  Please give credit where due.
*
*  The Author makes no warranty of any kind, express or implied, with regard
*  to this program or the documentation contained in this document.  The
*  Author shall not be liable in any event for incidental or consequential
*  damages in connection with, or arising out of, the furnishing, performance
*  or use of these programs.
*
*/

#include "ESPixelStick.h"
#include <FS.h>

#ifdef ARDUINO_ARCH_ESP32
#   include <LITTLEFS.h>
#   include <Update.h>
#else
#   include <LittleFS.h>
#   define LITTLEFS LittleFS
#endif

#define FORMAT_LITTLEFS_IF_FAILED true

/// Deserialization callback for I/O modules
typedef std::function<void (DynamicJsonDocument & json)> DeserializationHandler;

/// Contains static methods used for common File IO routines
class FileIO
{
public:

	static void listDir (fs::FS& fs, String dirname, uint8_t levels)
	{
		do // once
		{
			LOG_PORT.println (String (F ("Listing directory: ")) + dirname);

			File root = fs.open (dirname, "r");
			if (!root)
			{
				LOG_PORT.println (String (F ("failed to open directory: ")) + dirname);
				break;
			}

			if (!root.isDirectory ())
			{
				LOG_PORT.println (String (F ("Is not a directory: ")) + dirname);
				break;
			}

			File MyFile = root.openNextFile ();

			while (MyFile)
			{
				if (MyFile.isDirectory ())
				{
					if (levels)
					{
						listDir (fs, MyFile.name (), levels - 1);
					}
				}
				else
				{
					LOG_PORT.println ("'" + String (MyFile.name ()) + "': \t'" + String (MyFile.size ()) + "'");
				}
				MyFile = root.openNextFile ();
			}

		} while (false);

	} // listDir

	static void Begin ()
	{
		// DEBUG_START;

		do // once
		{
			if (!LITTLEFS.begin ())
			{
				LOG_PORT.println (F ("*** File system did not initialize correctly ***"));
				break;
			}

			LOG_PORT.println (F ("File system initialized."));

			listDir (LITTLEFS, String ("/"), 3);

		} while (false);

		// DEBUG_END;
	} // begin

  /// Load configuration file
  /** Loads JSON configuration file via File System.
   *  Returns true on success.
   */
	static boolean loadConfig (String & filename, DeserializationHandler dsHandler, size_t jsonSize = 3000)
	{
		boolean retval = false;

		fs::File file = LITTLEFS.open (filename.c_str (), "r");
		if (file)
		{
			retval = loadConfig (filename, file, dsHandler, jsonSize);

			file.close ();
		}
		else
		{
			LOG_PORT.println (String (F ("Configuration file: '")) + filename + String (F ("' not found.")));
		}

		return retval;

	} // loadConfig

	static boolean loadConfig (String & filename, fs::File & file, DeserializationHandler dsHandler, size_t jsonSize = 2048)
	{
		DynamicJsonDocument json (jsonSize);
		return loadConfig (filename, file, json, dsHandler);

	} // loadConfig
	
	static boolean loadConfig (String & filename, fs::File & file, DynamicJsonDocument & json, DeserializationHandler dsHandler)
	{
		// DEBUG_START;
		boolean retval = false;

		do // once
		{
			String CfgFileMessagePrefix = String (F ("Configuration file: '")) + filename + "' ";
			// DEBUG_V ("");
			if (!file)
			{
				LOG_PORT.println (CfgFileMessagePrefix + String (F ("not valid.")));
				break;
			}

			// DEBUG_V ("");

			// Parse CONFIG_FILE json
			if (file.size () > CONFIG_MAX_SIZE)
			{
				LOG_PORT.println (CfgFileMessagePrefix + String (F ("file is too large.")));
				break;
			}

			// DEBUG_V ("");
			file.seek (0);
			String buf = file.readString ();

			// DEBUG_V (buf);
			DeserializationError error = deserializeJson (json, buf);

			// DEBUG_V ("");
			if (error)
			{
				LOG_PORT.println (String(F("Heap:")) + String (ESP.getFreeHeap ()));
				LOG_PORT.println (CfgFileMessagePrefix + String (F ("Deserialzation Error. Error code = ")) + error.c_str ());
				LOG_PORT.println (String(F("++++")) + buf + String(F("----")));
				break;
			}

			// DEBUG_V ("");
			dsHandler (json);
			json.garbageCollect ();

			LOG_PORT.println (CfgFileMessagePrefix + String (F ("loaded.")));
			retval = true;

		} while (false);

		// DEBUG_END;
		return retval;
	} // loadConfig

	static boolean ReadFile (const String & filename, String & Result)
	{
		// DEBUG_START;
		boolean retval = false;

		do // once
		{
			String CfgFileMessagePrefix = String (F ("File: '")) + filename + "' ";
			// DEBUG_V ("");
			fs::File file = LITTLEFS.open (filename.c_str (), "r");
			if (!file)
			{
				LOG_PORT.println (CfgFileMessagePrefix + String (F ("not valid.")));
				break;
			}

			// DEBUG_V ("");

			// Parse CONFIG_FILE json
			size_t size = file.size ();
			if (size > CONFIG_MAX_SIZE)
			{
				LOG_PORT.println (CfgFileMessagePrefix + String (F ("too large.")));
			}

			// DEBUG_V ("");
			Result = file.readString ();
			// DEBUG_V (Result);

			// DEBUG_V ("");

			LOG_PORT.println (CfgFileMessagePrefix + String (F ("loaded.")));
			retval = true;

		} while (false);

		// DEBUG_END;
		return retval;

	} // loadConfig

	/// Save configuration file
	/** Saves configuration file via File System
	 * Returns true on 'value has been changed'.
	 * Returns false if field not found
	 * Returns false if value is unchanged
	 */
	static boolean SaveConfig (const String & filename, String & jsonString)
	{
		boolean retval = false;
		String CfgFileMessagePrefix = String (F ("Configuration file: '")) + filename + "' ";

		fs::File file = LITTLEFS.open (filename.c_str (), "w");
		if (!file)
		{
			LOG_PORT.println (CfgFileMessagePrefix + String (F ("Could not open file for writing..")));
		}
		else
		{
			file.print (jsonString);
			LOG_PORT.println (CfgFileMessagePrefix + String (F ("saved.")));
			retval = true;

			file.close ();
		}

		return retval;

	} // SaveConfig

	  /// Checks if value is empty and sets key to value if they're different. Returns true if key was set
	static boolean setFromJSON (String   & key, JsonVariant val) {
		if (!val.isNull () && !val.as<String> ().equals (key)) {
			//LOG_PORT.printf("**** Setting '%s' ****\n", val.c_str());
			key = val.as<String> ();
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (boolean  & key, JsonVariant val) {
		if (!val.isNull () && (val.as<boolean> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (uint8_t  & key, JsonVariant val) {
		if (!val.isNull () && (val.as<uint8_t> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (uint16_t & key, JsonVariant val) {
		if (!val.isNull () && (val.as<uint16_t> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (uint32_t & key, JsonVariant val) {
		if (!val.isNull () && (val.as<uint32_t> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (float& key, JsonVariant val) {
		if (!val.isNull () && (val.as<float> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}

	static boolean setFromJSON (int& key, JsonVariant val) {
		if (!val.isNull () && (val.as<int> () != key)) {
			key = val;
			return true;
		}
		else {
			return false;
		}
	}
};

