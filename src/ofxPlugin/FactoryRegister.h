#pragma once

#include "Factory.h"

#include "ofAppRunner.h"

#include "ofxSingleton.h"

#include <Windows.h>
#include <map>

namespace ofxPlugin {
	template<typename ModuleBaseType>
	class FactoryRegister : public std::map<std::string, std::shared_ptr<BaseFactory<ModuleBaseType>>> {
	public:
		//----------
		struct PluginInitArgs {
			std::shared_ptr<ofMainLoop> mainLoop;
			FactoryRegister<ModuleBaseType> * factoryRegister;
			ofxSingleton::Register * singletonRegister;
		};

		//----------
		typedef void(*InitPluginFunctionType)(PluginInitArgs *);
		typedef const char *(*GetPluginTypeNameType)();

		//----------
		typedef ofxPlugin::BaseFactory<ModuleBaseType> BaseFactory;

		//----------
		///Add a factory which you already have to the register
		void add(std::shared_ptr<BaseFactory> baseFactory) {
			this->insert(pair<std::string, std::shared_ptr<BaseFactory>>(baseFactory->getModuleTypeName(), baseFactory));
		}

		//----------
		///Create a factory for a specific type (defined in the template arguments of the function call) and add it to the register.
		template<typename ModuleType>
		void add() {
			this->add(std::make_shared<Factory<ModuleType, ModuleBaseType>>());
		}

		//----------
		///Get a factory for a specific type (defined in the template arguments of the function call). Returns an empty pointer if factory wasn't found
		template<typename ModuleType>
		std::shared_ptr<ModuleBaseType> get() {
			auto moduleTypeName = ModuleType().getTypeName();
			return this->get(moduleTypeName)
		}

		//----------
		///Get a factory for a specific type (defined by moduleNameType). Returns an empty pointer if factory wasn't found
		std::shared_ptr<BaseFactory> get(std::string moduleTypeName) {
			auto findFactory = this->find(moduleTypeName);
			if (findFactory == this->end()) {
				return shared_ptr<BaseFactory>();
			}
			else {
				return findFactory->second;
			}
		}

		//----------
		// from http://www.codeproject.com/Tips/479880/GetLastError-as-std-string
		std::string GetLastErrorStdStr()
		{
			DWORD error = GetLastError();
			if (error)
			{
				LPVOID lpMsgBuf;
				DWORD bufLen = FormatMessageA(
					FORMAT_MESSAGE_ALLOCATE_BUFFER |
					FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS,
					NULL,
					error,
					MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPSTR)&lpMsgBuf,
					0, NULL);
				if (bufLen)
				{
					LPCSTR lpMsgStr = (LPCSTR)lpMsgBuf;
					std::string result(lpMsgStr, lpMsgStr + bufLen);

					LocalFree(lpMsgBuf);

					return result;
				}
			}
			return std::string();
		}

		//----------
		///Load any factories from the plugin which match this FactoryRegister
		bool loadPlugin(std::filesystem::path path, bool verbose = false) {
			//transform path to data path
			if (path.is_relative()) {
				path = std::filesystem::path(ofToDataPath("")) / path;
			}

			//attempt to load DLL
			auto dll = LoadLibraryW(path.wstring().c_str());
			if (!dll) {
				auto errorMessage = GetLastErrorStdStr();
				ofLogWarning("ofxPlugin") << "Failed to open DLL [" << path << "]. " << errorMessage;
				return false;
			}

			//check if dll contains plugin and what type it is
			auto getPluginTypeName = (GetPluginTypeNameType)GetProcAddress(dll, "getPluginTypeName");
			if (!getPluginTypeName) {
				if (verbose) {
					ofLogWarning("ofxPlugin") << "This DLL file is not a plugin";
				}
				FreeLibrary(dll);
				return false;
			}
			auto pluginTypeName = getPluginTypeName();
			auto ourTypeName = typeid(ModuleBaseType).name();
			if (string(pluginTypeName) != string(ourTypeName)) {
				if (verbose) {
					ofLogWarning("ofxPlugin") << "This DLL file contains a plugin of the incorrect type (" << pluginTypeName << "), we are (" << ourTypeName << ")";
				}
				FreeLibrary(dll);
				return false;
			}

			//attempt to initialise plugin
			auto initPlugin = (InitPluginFunctionType) GetProcAddress(dll, "initPlugin");
			if (!initPlugin) {
				if (verbose) {
					ofLogWarning("ofxPlugin") << "This DLL file is not a plugin";
				}
				FreeLibrary(dll);
				return false;
			}
			
			//we are currently in the main application
			//the initialisation arguments are the 'ofMainLoop' singleton, this factory register, and the 
			auto singletonRegisterRawPointer = ofxSingleton::Register::X().getInstance().get();
			PluginInitArgs pluginInitArgs = {
				ofGetMainLoop(),
				this,
				singletonRegisterRawPointer
			};
			initPlugin(&pluginInitArgs);

			return true;
		}

		//----------
		///Look within a path for dll's and try and find plugins there
		void loadPlugins(string searchPath = "../", bool verbose = true) {
			if (verbose) {
				cout << "--" << endl;
				cout << "Loading plugins of type [" << typeid(ModuleBaseType).name() << "] begin" << endl;
			}
			auto exePath = ofToDataPath(searchPath, true);
			for (auto & entry : std::filesystem::directory_iterator(exePath)) {
				const auto extension = entry.path().extension();
				if (ofToLower(extension.string()) == ".dll") {
					try {
						this->loadPlugin(entry.path(), false); // try to load it as a plugin
					}
					catch (std::exception e) {
						ofLogError("ofxPlugin") << "Failed to load '" << entry.path() << "' : " << e.what();
					}
				}
			}

			if (verbose) {
				cout << "Loading plugins of type [" << typeid(ModuleBaseType).name() << "] end" << endl;
				cout << "--" << endl;
			}
		}
	};
}