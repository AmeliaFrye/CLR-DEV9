#include "PSE.h"

//#include <dirent.h>
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
//#include <fcntl.h>
//#include <sys/types.h>
//#include <sys/stat.h>

using namespace std;

//Public
MonoImage *pluginImage = NULL;
MonoDomain* pluginDomain = NULL; //Plugin Specific Domain

//coreclr_create_delegate_ptr createDelegate;
//Helper Methods
ThunkGetDelegate CyclesCallbackFromFunctionPointer;
ThunkGetFuncPtr FunctionPointerFromIRQHandler;

PluginLog PSELog;

//Private
//Is a char* because strings will init
//after constuctors are called
const char* pseDomainName = "PSE_Mono";

MonoDomain* pseDomain = NULL; //Base Domain
//PluginDomain
MonoAssembly* pluginAssembly = NULL; //Plugin
//PluginImage

typedef MonoString*(*ThunkGetLibName)(MonoException** ex);
ThunkGetLibName managedGetLibName;

typedef uint32_t(*ThunkGetLibType)(MonoException** ex);
ThunkGetLibType managedGetLibType;

typedef uint32_t(*ThunkGetLibVersion2)(uint32_t type, MonoException** ex);
ThunkGetLibVersion2 managedGetLibVersion2;

static char* pluginNamePtr = NULL;

EXPORT_C_(const char*)
PS2EgetLibName(void)
{
	MonoException* ex;
	MonoString* ret = managedGetLibName(&ex);

	if (pluginNamePtr != NULL)
	{
		mono_free(pluginNamePtr);
		pluginNamePtr = NULL;
	}

	if (ex)
	{
		mono_print_unhandled_exception((MonoObject*)ex);
		throw;
	}

	pluginNamePtr = mono_string_to_utf8(ret);

	return pluginNamePtr;
}

EXPORT_C_(uint32_t)
PS2EgetLibType(void)
{
	MonoException* ex;
	uint32_t ret = managedGetLibType(&ex);

	if (ex)
	{
		mono_print_unhandled_exception((MonoObject*)ex);
		throw;
	}

	return ret;
}

EXPORT_C_(uint32_t)
PS2EgetLibVersion2(uint32_t type)
{
	MonoException* ex;
	uint32_t ret = managedGetLibVersion2(type, &ex);

	if (ex)
	{
		mono_print_unhandled_exception((MonoObject*)ex);
		throw;
	}

	return ret;
}

//EXPORT_C_(void)
//TestInit()
//{
//	LoadCoreCLR("/home/air/.config/PCSX2/inis_1.4.0/CLR_DEV9_CORE.dll", "", "");
//	CloseCoreCLR();
//}

////HACKFIX (The PipeCleaner)
////PCSX2 will hang if extra stdout/stderr handles
////opened by CoreCLR are left open after shutdown
//string p1;
//string p2;
//vector<int32_t> p1FDs;
//vector<int32_t> p2FDs;
//vector<int32_t> p1ClrFDs;
//vector<int32_t> p2ClrFDs;
//
//string GetFDname(int32_t fd)
//{
//	char buf[256];
//
//	int32_t fd_flags = fcntl(fd, F_GETFD);
//	if (fd_flags == -1) return "";
//
//	int32_t fl_flags = fcntl(fd, F_GETFL);
//	if (fl_flags == -1) return "";
//
//	char path[256];
//	sprintf(path, "/proc/self/fd/%d", fd);
//
//	memset(&buf[0], 0, 256);
//	ssize_t s = readlink(path, &buf[0], 256);
//	if (s == -1)
//	{
//		return "";
//	}
//	return string(buf);
//}
//
//vector<int32_t> FindAllFDForFile(string path)
//{
//	int32_t numHandles = getdtablesize();
//
//	vector<int32_t> FDs;
//
//	for (int32_t i = 0; i < numHandles; i++)
//	{
//		int32_t fd_flags = fcntl(i, F_GETFD);
//		if (fd_flags == -1) continue;
//
//		string ret = GetFDname(i);
//		if (path.compare(ret) == 0)
//		{
//			FDs.push_back(i);
//		}
//	}
//
//	return FDs;
//}
//
//void LoadInitialFD()
//{
//	p1 = GetFDname(1);
//	p2 = GetFDname(2);
//	p1FDs = FindAllFDForFile(p1);
//	p2FDs = FindAllFDForFile(p2);
//}
//
//void LoadExtraFD()
//{
//	vector<int32_t> newP1FDs = FindAllFDForFile(p1);
//	vector<int32_t> newP2FDs = FindAllFDForFile(p2);
//
//	if (newP1FDs.size() > p1FDs.size())
//	{
//		PSELog.Write("%s has %d extra open handle(s)\n", p1.c_str(), newP1FDs.size() - p1FDs.size());
//		vector<int32_t> excessFDs;
//		for (size_t x = 0; x < newP1FDs.size(); x++)
//		{
//			bool old = false;
//			for (size_t y = 0; y < p1FDs.size(); y++)
//			{
//				if (newP1FDs[x] == p1FDs[y])
//					old = true;
//			}
//			if (old == false)
//				excessFDs.push_back(newP1FDs[x]);
//		}
//		p1ClrFDs = excessFDs;
//		//for (size_t x = 0; x < excessFDs.size(); x++)
//		//{
//		//	PSELog.Write("Closing %d\n", excessFDs[x]);
//		//	close(excessFDs[x]);
//		//}
//	}
//
//	if (newP2FDs.size() > p2FDs.size())
//	{
//		PSELog.Write("%s has %d extra open handle(s)\n", p2.c_str(), newP2FDs.size() - p2FDs.size());
//		vector<int32_t> excessFDs;
//		for (size_t x = 0; x < newP2FDs.size(); x++)
//		{
//			bool old = false;
//			for (size_t y = 0; y < p2FDs.size(); y++)
//			{
//				if (newP2FDs[x] == p2FDs[y])
//					old = true;
//			}
//			if (old == false)
//				excessFDs.push_back(newP2FDs[x]);
//		}
//		p2ClrFDs = excessFDs;
//		//for (size_t x = 0; x < excessFDs.size(); x++)
//		//{
//		//	PSELog.Write("Closing %d\n", excessFDs[x]);
//		//	close(excessFDs[x]);
//		//}
//	}
//}
//
//void CloseCLRFD()
//{
//	for (size_t x = 0; x < p1ClrFDs.size(); x++)
//	{
//		int32_t fd_flags = fcntl(p1ClrFDs[x], F_GETFD);
//		if (fd_flags == -1) continue;
//		PSELog.Write("%d is still open, closing\n", p1ClrFDs[x]);
//		close(p1ClrFDs[x]);
//	}
//	for (size_t x = 0; x < p2ClrFDs.size(); x++)
//	{
//		int32_t fd_flags = fcntl(p2ClrFDs[x], F_GETFD);
//		if (fd_flags == -1) continue;
//		PSELog.Write("%d is still open, closing\n", p2ClrFDs[x]);
//		close(p2ClrFDs[x]);
//	}
//}
////end HACKFIX (The PipeCleaner)

void LoadCoreCLR(char* pluginData, size_t pluginLength, string monoUsrLibFolder, string monoEtcFolder)
{
	PSELog.WriteLn("Init Mono Runtime");

	if (pluginDomain != NULL)
	{
		//Check pluginImage
		return;
	}

	//Check if mono is already active
	pseDomain = mono_get_root_domain();
		
	if (pseDomain == NULL)
	{
		//Inc Reference
		dlopen("libmono-2.0.so", RTLD_NOW | RTLD_LOCAL);

		//LoadInitialFD();

		PSELog.WriteLn("Set Dirs");

		if (monoUsrLibFolder.length() == 0)
		{
			monoUsrLibFolder = "/usr/lib/";
		}
		if (monoEtcFolder.length() == 0)
		{
			monoEtcFolder = "/etc/";
		}

		mono_set_dirs(monoUsrLibFolder.c_str(), monoEtcFolder.c_str());
		mono_config_parse(NULL);
		mono_set_signal_chaining(true);

		PSELog.Write("Set Debug (if only)\n");
		mono_debug_init(MONO_DEBUG_FORMAT_MONO);

		//const char* options[] = {
		//	"--debugger-agent=transport=dt_socket,server=y,address=127.0.0.1:55555"
		//};
		//mono_jit_parse_options(1, (char**)options);

		PSELog.Write("JIT Init\n");
		pseDomain = mono_jit_init(pseDomainName);
		if (pseDomain == NULL)
		{
			PSELog.Write("Init Mono Failed At jit_init\n");
			return;
		}
		else
		{
			PSELog.WriteLn(mono_domain_get_friendly_name(pseDomain));
		}
	}
	else
	{
		PSELog.WriteLn("Mono Already Running");
		PSELog.WriteLn(mono_domain_get_friendly_name(pseDomain));
	}

	//pluginDomain = mono_domain_create_appdomain("Test", "machine.config");//mono_domain_create();

	pluginDomain = pseDomain;

	//if (!mono_domain_set(pluginDomain, false))
	//{
	//	PSELog.WriteLn("Set Domain Failed");
	//	CloseCoreCLR();
	//	return;
	//}

	PSELog.WriteLn("Load Image");
	MonoImageOpenStatus status;
	pluginImage = mono_image_open_from_data_full(pluginData, pluginLength, true, &status, false);
	mono_image_addref(pluginImage);
	if (!pluginImage | (status != MONO_IMAGE_OK))
	{
		PSELog.WriteLn("Init Mono Failed At PluginImage");
		CloseCoreCLR();
		return;
	}

	PSELog.WriteLn("Load Assembly");
	pluginAssembly = mono_assembly_load_from_full(pluginImage, "", &status, false);

	if (!pluginAssembly | (status != MONO_IMAGE_OK))
	{
		PSELog.WriteLn("Init Mono Failed At PluginAssembly");
		CloseCoreCLR();
		return;
	}

	PSELog.WriteLn("Get PSE classes");

	MonoClass *pseClass;
	MonoClass *pseClass_mono;

	pseClass = mono_class_from_name(pluginImage, "PSE", "CLR_PSE");
	pseClass_mono = mono_class_from_name(pluginImage, "PSE", "CLR_PSE_Mono");

	if (!pseClass | !pseClass_mono)
	{
		PSELog.WriteLn("Failed to load CLR_PSE classes");
		CloseCoreCLR();
		return;
	}

	PSELog.WriteLn("Set Main Args()");

	char pcsx2Path[PATH_MAX];
	size_t len = readlink("/proc/self/exe", pcsx2Path, PATH_MAX - 1);
	if (len < 0)
	{
		PSELog.Write("Init CLR Failed At readlink\n");
		CloseCoreCLR();
		return;
	}

	pcsx2Path[len] = 0;
	PSELog.WriteLn("PCSX2 Path is %s", pcsx2Path);

	char* argv[1];
	argv[0] = pcsx2Path;//(char *)pluginPath.c_str();

	int32_t ret = mono_runtime_set_main_args(1, argv);
	mono_domain_set_config(pluginDomain, ".", "");

	if (ret != 0)
	{
		CloseCoreCLR();
		return;
	}

	PSELog.WriteLn("Get PSE Methods");

	MonoMethod* meth;

	meth = mono_class_get_method_from_name(pseClass, "PS2EgetLibName", 0);
	managedGetLibName = (ThunkGetLibName)mono_method_get_unmanaged_thunk(meth);

	meth = mono_class_get_method_from_name(pseClass, "PS2EgetLibType", 0);
	managedGetLibType = (ThunkGetLibType)mono_method_get_unmanaged_thunk(meth);
	
	meth = mono_class_get_method_from_name(pseClass, "PS2EgetLibVersion2", 1);
	managedGetLibVersion2 = (ThunkGetLibVersion2)mono_method_get_unmanaged_thunk(meth);

	PSELog.WriteLn("Get helpers");

	meth = mono_class_get_method_from_name(pseClass_mono, "CyclesCallbackFromFunctionPointer", 1);
	CyclesCallbackFromFunctionPointer = (ThunkGetDelegate)mono_method_get_unmanaged_thunk(meth);

	meth = mono_class_get_method_from_name(pseClass_mono, "FunctionPointerFromIRQHandler", 1);
	FunctionPointerFromIRQHandler = (ThunkGetFuncPtr)mono_method_get_unmanaged_thunk(meth);

	PSELog.WriteLn("Init CLR Done");
}

void CloseCoreCLR()
{
	if (pluginNamePtr != NULL)
	{
		mono_free(pluginNamePtr);
		pluginNamePtr = NULL;
	}
	if (pluginAssembly != NULL)
	{
		mono_assembly_close(pluginAssembly);
		pluginAssembly = NULL;
	}
	if (pluginImage != NULL)
	{
		mono_image_close(pluginImage);
		pluginImage = NULL;
	}
	if (pluginDomain != NULL)
	{
		//mono_domain_unload(pluginDomain);
		//mono_domain_free(pluginDomain,false);
		pluginDomain = NULL;
	}

	//LoadExtraFD();
	//CloseCLRFD();
}
