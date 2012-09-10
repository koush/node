// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.


#include "node.h"
#include "node_os.h"

#include "v8.h"

#include <errno.h>
#include <string.h>

#ifdef __MINGW32__
# include <io.h>
#endif

#ifdef __POSIX__
# include <unistd.h>  // gethostname, sysconf
# include <sys/utsname.h>
#endif

namespace node {

using namespace v8;

static Handle<Value> GetHostname(const Arguments& args) {
  HandleScope scope;
  char s[255];
  int r = gethostname(s, 255);

  if (r < 0) {
#ifdef __POSIX__
    return ThrowException(ErrnoException(errno, "gethostname"));
#else // __MINGW32__
    return ThrowException(ErrnoException(WSAGetLastError(), "gethostname"));
#endif // __MINGW32__
  }

  return scope.Close(String::New(s));
}

static Handle<Value> GetOSType(const Arguments& args) {
  HandleScope scope;

#ifdef __POSIX__
  char type[256];
  struct utsname info;

  uname(&info);
  strncpy(type, info.sysname, strlen(info.sysname));
  type[strlen(info.sysname)] = 0;

  return scope.Close(String::New(type));
#else // __MINGW32__
  return scope.Close(String::New("Windows_NT"));
#endif
}

static Handle<Value> GetOSRelease(const Arguments& args) {
  HandleScope scope;
  char release[256];

#ifdef __POSIX__
  struct utsname info;

  uname(&info);
  strncpy(release, info.release, strlen(info.release));
  release[strlen(info.release)] = 0;

#else // __MINGW32__
  OSVERSIONINFO info;
  info.dwOSVersionInfoSize = sizeof(info);

  if (GetVersionEx(&info) == 0) {
    return Undefined();
  }

  sprintf(release, "%d.%d.%d", static_cast<int>(info.dwMajorVersion),
      static_cast<int>(info.dwMinorVersion), static_cast<int>(info.dwBuildNumber));
#endif

  return scope.Close(String::New(release));
}

static Handle<Value> GetCPUInfo(const Arguments& args) {
  HandleScope scope;
  uv_cpu_info_t* cpu_infos;
  int count, i;

  uv_err_t err = uv_cpu_info(&cpu_infos, &count);

  if (err.code != UV_OK) {
    return Undefined();
  }

  Local<Array> cpus = Array::New();

  for (i = 0; i < count; i++) {
    Local<Object> times_info = Object::New();
    times_info->Set(String::New("user"),
      Integer::New(cpu_infos[i].cpu_times.user));
    times_info->Set(String::New("nice"),
      Integer::New(cpu_infos[i].cpu_times.nice));
    times_info->Set(String::New("sys"),
      Integer::New(cpu_infos[i].cpu_times.sys));
    times_info->Set(String::New("idle"),
      Integer::New(cpu_infos[i].cpu_times.idle));
    times_info->Set(String::New("irq"),
      Integer::New(cpu_infos[i].cpu_times.irq));

    Local<Object> cpu_info = Object::New();
    cpu_info->Set(String::New("model"), String::New(cpu_infos[i].model));
    cpu_info->Set(String::New("speed"), Integer::New(cpu_infos[i].speed));
    cpu_info->Set(String::New("times"), times_info);
    (*cpus)->Set(i,cpu_info);
  }

  uv_free_cpu_info(cpu_infos, count);

  return scope.Close(cpus);
}

static Handle<Value> GetFreeMemory(const Arguments& args) {
  HandleScope scope;
  double amount = uv_get_free_memory();

  if (amount < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(amount));
}

static Handle<Value> GetTotalMemory(const Arguments& args) {
  HandleScope scope;
  double amount = uv_get_total_memory();

  if (amount < 0) {
    return Undefined();
  }

  return scope.Close(Number::New(amount));
}

static Handle<Value> GetUptime(const Arguments& args) {
  HandleScope scope;
  double uptime;

  uv_err_t err = uv_uptime(&uptime);

  if (err.code != UV_OK) {
    return Undefined();
  }

  return scope.Close(Number::New(uptime));
}

static Handle<Value> GetLoadAvg(const Arguments& args) {
  HandleScope scope;
  double loadavg[3];
  uv_loadavg(loadavg);

  Local<Array> loads = Array::New(3);
  loads->Set(0, Number::New(loadavg[0]));
  loads->Set(1, Number::New(loadavg[1]));
  loads->Set(2, Number::New(loadavg[2]));

  return scope.Close(loads);
}


static Handle<Value> GetInterfaceAddresses(const Arguments& args) {
  HandleScope scope;
  uv_interface_address_t* interfaces;
  int count, i;
  char ip[INET6_ADDRSTRLEN];
  Local<Object> ret, o;
  Local<String> name, family;
  Local<Array> ifarr;

  uv_err_t err = uv_interface_addresses(&interfaces, &count);

  if (err.code != UV_OK) {
    return Undefined();
  }

  ret = Object::New();

  for (i = 0; i < count; i++) {
    name = String::New(interfaces[i].name);
    if (ret->Has(name)) {
      ifarr = Local<Array>::Cast(ret->Get(name));
    } else {
      ifarr = Array::New();
      ret->Set(name, ifarr);
    }

    if (interfaces[i].address.address4.sin_family == AF_INET) {
      uv_ip4_name(&interfaces[i].address.address4,ip, sizeof(ip));
      family = String::New("IPv4");
    } else if (interfaces[i].address.address4.sin_family == AF_INET6) {
      uv_ip6_name(&interfaces[i].address.address6, ip, sizeof(ip));
      family = String::New("IPv6");
    } else {
      strncpy(ip, "<unknown sa family>", INET6_ADDRSTRLEN);
      family = String::New("<unknown>");
    }

    o = Object::New();
    o->Set(String::New("address"), String::New(ip));
    o->Set(String::New("family"), family);
    o->Set(String::New("internal"), interfaces[i].is_internal ?
	True() : False());

    ifarr->Set(ifarr->Length(), o);
  }

  uv_free_interface_addresses(interfaces, count);

  return scope.Close(ret);
}



#ifdef __POSIX__
#ifdef __APPLE__
static Handle<Value> SetupTun(const Arguments& args) {
    return v8::Handle<v8::Value>();
}
#else
#include <net/if.h>
#include <linux/if_tun.h>
#include <memory.h>
#include <stropts.h>
#include <asm-generic/ioctl.h>

static Handle<Value> SetupTun(const Arguments& args) {
  HandleScope scope;
  if (args.Length() > 0) {
      Local<Value> value = args[0];
      if (value->IsNumber()) {
          Local<Number> number = value->ToNumber();
          int fd = (int)number->Value();
          struct ifreq ifr;
          memset(&ifr, 0, sizeof(ifr));
          ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
          int err;
          strncpy(ifr.ifr_name, "tun", IFNAMSIZ);
          if ((err = ioctl(fd, TUNSETIFF, (void *)&ifr)) < 0) {
              return String::New("Failure during ioctl.");
          }
          return Handle<Value>();
      }
  }
  return String::New("No fd provided.");
  return v8::Handle<v8::Value>();
}
#endif

#else // __MINGW32__

static char* GetDeviceGuid()
{
  const char* AdapterKey = "SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}";
  HKEY adapter_key;
  if (ERROR_SUCCESS != RegOpenKeyExA(HKEY_LOCAL_MACHINE, AdapterKey, 0, KEY_READ, &adapter_key))
    return NULL;
  int index = 0;
  char keyName[256];
  while (ERROR_SUCCESS == RegEnumKeyA(adapter_key, index++, keyName, 256))
  {
    HKEY reg_adapter;
    if (ERROR_SUCCESS != RegOpenKeyA(adapter_key, keyName, &reg_adapter))
      continue;
    char value[256];
    DWORD len = 256;
    if (ERROR_SUCCESS != RegQueryValueExA(reg_adapter, "ComponentId", NULL, NULL, reinterpret_cast<LPBYTE>(value), &len))
    {
      RegCloseKey(reg_adapter);
      continue;
    }
    if (strcmp("tap0901", value) == 0)
    {
      char* ret = (char*)malloc(256);
      len = 256;
      if (ERROR_SUCCESS != RegQueryValueExA(reg_adapter, "NetCfgInstanceId", NULL, NULL, reinterpret_cast<LPBYTE>(ret), &len))
      {
        RegCloseKey(reg_adapter);
        free(ret);
        return NULL;
      }
      RegCloseKey(reg_adapter);
      RegCloseKey(adapter_key);
      return ret;
    }
  }
  RegCloseKey(adapter_key);
  return NULL;
}

static char* GetNetworkName(const char* guid)
{
  char ConnectionKey[256];
  sprintf(ConnectionKey, "SYSTEM\\CurrentControlSet\\Control\\Network\\{4D36E972-E325-11CE-BFC1-08002BE10318}\\%s\\Connection", guid);
  HKEY connection_key;
  if (ERROR_SUCCESS != RegOpenKeyExA(HKEY_LOCAL_MACHINE, ConnectionKey, 0, KEY_READ, &connection_key))
    return NULL;
  
  char* ret = (char*)malloc(256);
  DWORD len = 256;
  if (ERROR_SUCCESS != RegQueryValueExA(connection_key, "Name", NULL, NULL, reinterpret_cast<LPBYTE>(ret), &len))
  {
    free(ret);
    return NULL;
  }

  return ret;
}

#define TAP_CONTROL_CODE(request, method) CTL_CODE(FILE_DEVICE_UNKNOWN, request, method, FILE_ANY_ACCESS)

static Handle<Value> SetupTun(const Arguments& args) {
  char* deviceGuid = GetDeviceGuid();
  if (NULL == deviceGuid)
    return String::New("Error retrieving device guid. Is the tap driver installed?");

  char* networkName = GetNetworkName(deviceGuid);
  if (NULL == networkName) {
    free(deviceGuid);
    return String::New("Error retrieving network name for tap device.");
  }

  char deviceFile[256];
  sprintf(deviceFile, "\\\\.\\Global\\%s.tap", deviceGuid);
  free(deviceGuid);

  HANDLE fd = CreateFileA(deviceFile, FILE_WRITE_ACCESS | FILE_READ_ACCESS, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM | FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH, NULL);
  if (INVALID_HANDLE_VALUE == fd) {
    free(networkName);
    return String::New("Error opening tap file. Are you running as root?");
  }

  DWORD len;
  int status = 1;
  DeviceIoControl(fd, TAP_CONTROL_CODE(6, METHOD_BUFFERED), &status, 4, &status, 4, &len, NULL);

  int data[3];
  int ip = 0x0100000a;
  int network = 0x0000000a;
  int netmask = 0x00ffffff;
  data[0] = ip;
  data[1] = network;
  data[2] = netmask;
  DeviceIoControl(fd, TAP_CONTROL_CODE(10, METHOD_BUFFERED), data, 12, data, 12, &len, NULL);

  // int result = _open_osfhandle(fd, O_RDWR);
  // return Integer::New(result);

  return Integer::New(-reinterpret_cast<int>(fd));
}

#endif

void OS::Initialize(v8::Handle<v8::Object> target) {
  HandleScope scope;

  NODE_SET_METHOD(target, "getHostname", GetHostname);
  NODE_SET_METHOD(target, "getLoadAvg", GetLoadAvg);
  NODE_SET_METHOD(target, "getUptime", GetUptime);
  NODE_SET_METHOD(target, "getTotalMem", GetTotalMemory);
  NODE_SET_METHOD(target, "getFreeMem", GetFreeMemory);
  NODE_SET_METHOD(target, "getCPUs", GetCPUInfo);
  NODE_SET_METHOD(target, "getOSType", GetOSType);
  NODE_SET_METHOD(target, "getOSRelease", GetOSRelease);
  NODE_SET_METHOD(target, "getInterfaceAddresses", GetInterfaceAddresses);
  NODE_SET_METHOD(target, "setupTun", SetupTun);
}


}  // namespace node

NODE_MODULE(node_os, node::OS::Initialize)
