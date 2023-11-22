#include <napi.h>
#include <string>
#include <uv.h>
#include <windows.h>
#include <winternl.h>

using namespace Napi;
#define VALUE_MAX_SIZE 32767
#define DATA_MAX_SIZE (1024 * 1024)


typedef NTSTATUS(NTAPI* _NtQueryInformationProcess)(
  HANDLE ProcessHandle,
  PROCESSINFOCLASS ProcessInformationClass,
  PVOID ProcessInformation,
  ULONG ProcessInformationLength,
  PULONG ReturnLength
);

Napi::Value throwError(std::string what,  const Napi::Env &env) {
  Napi::Error::New(env, what).ThrowAsJavaScriptException();
  return env.Null();
}

Napi::Value getWorkingDirectory(const HANDLE hProcess, const Napi::Env &env) {
  HMODULE ntdllHandle = GetModuleHandle(L"ntdll.dll");
  if (!ntdllHandle) {
    return throwError("Cannot get handle to ntdll.dll", env);
  }
  _NtQueryInformationProcess NtQueryInformationProcess = (_NtQueryInformationProcess)GetProcAddress(ntdllHandle, "NtQueryInformationProcess");

  BOOL wow;
  IsWow64Process(GetCurrentProcess(), &wow);
  if (wow) {
    return throwError("Cannot fetch current directory for WoW64 process", env);
  }

  PROCESS_BASIC_INFORMATION pbi;

  NTSTATUS status = NtQueryInformationProcess(hProcess, ProcessBasicInformation, &pbi, sizeof(pbi), NULL);
  if (!NT_SUCCESS(status)) {
    return throwError("NtQueryInformationProcess failed to fetch ProcessBasicInformation" + std::to_string(status), env);
  }

  PEB peb;
  if (!ReadProcessMemory(hProcess, pbi.PebBaseAddress, &peb, sizeof(peb), NULL))
  {
    return throwError("Failed to read PROCESS_BASIC_INFORMATION.PebBaseAddress", env);
  }

  RTL_USER_PROCESS_PARAMETERS procParams;
  if (!ReadProcessMemory(hProcess, peb.ProcessParameters, &procParams, sizeof(procParams), NULL))
  {
    return throwError("Failed to read PEB.ProcessParameters", env);
  }

  UNICODE_STRING currentDirUnicodeStr = *(UNICODE_STRING*)((PCHAR)&procParams + 0x38);

  if (currentDirUnicodeStr.Length <= 0 || currentDirUnicodeStr.MaximumLength <= 0
    || currentDirUnicodeStr.Length >= currentDirUnicodeStr.MaximumLength || currentDirUnicodeStr.MaximumLength > 8192) {
    return throwError("Bad current directory: Length=" + std::to_string(currentDirUnicodeStr.Length)
      + ", MaximumLength=" + std::to_string(currentDirUnicodeStr.MaximumLength), env);
  }

  LPWSTR lpCurrentDir = new WCHAR[currentDirUnicodeStr.MaximumLength / sizeof(WCHAR) + 1];
  if (!ReadProcessMemory(hProcess, currentDirUnicodeStr.Buffer, lpCurrentDir, currentDirUnicodeStr.MaximumLength, NULL))
  {
    delete[] lpCurrentDir;
    return throwError("Failed to read ProcessParameters.CurrentDirectory", env);
  }

  auto result = Napi::String::New(env, reinterpret_cast<char16_t*>(lpCurrentDir));
  delete[] lpCurrentDir;
  return result;
}

Napi::Value getWorkingDirectoryFromPID(const Napi::CallbackInfo& info) {
  auto pid = (int64_t)info[0].ToNumber();

  HANDLE h = OpenProcess(PROCESS_ALL_ACCESS, TRUE, pid);
  auto result = getWorkingDirectory(h, info.Env());
  CloseHandle(h);

  return result;
}

Napi::Value getWorkingDirectoryFromHandle(const Napi::CallbackInfo& info) {
  auto handle = (int64_t)info[0].ToNumber();
  return getWorkingDirectory((HANDLE)handle, info.Env());
}


// Function to open a registry key with specified access rights
HKEY openKey(const Napi::CallbackInfo& info, REGSAM access) {
  HKEY ret = 0;
  auto root = (HKEY)(int64_t)info[0].ToNumber();
  auto path = info[1].ToString().Utf16Value();

  LSTATUS error;
  if ((error = RegOpenKeyExW(root, (LPCWSTR)path.c_str(), 0, access, &ret)) != ERROR_SUCCESS) {
    return 0;
  }
  return ret;
}

static WCHAR name[VALUE_MAX_SIZE];
static BYTE data[DATA_MAX_SIZE];

// Function to get a registry key and its values
Napi::Value getKey(const Napi::CallbackInfo& info) {
  auto env = info.Env();

  auto key = openKey(info, KEY_READ);
  if (!key) {
    return info.Env().Null();
  }

  DWORD index = 0;
  DWORD valueType;
  DWORD nameLength;
  DWORD dataLength;

  auto ret = Array::New(env);

  LSTATUS error;

  // Loop through all values of the key
  while (TRUE) {
    nameLength = VALUE_MAX_SIZE - 1;
    dataLength = DATA_MAX_SIZE - 1;
    if ((error = RegEnumValueW(key, index, (LPWSTR)&name, &nameLength, NULL, &valueType, (LPBYTE)&data, &dataLength)) != ERROR_SUCCESS) {
      if (error == ERROR_NO_MORE_ITEMS) {
        break;
      }
      return info.Env().Null();
    }

    // Create a JavaScript object for each value
    auto obj = Object::New(env);
    auto jsName = String::New(env, reinterpret_cast<char16_t*>(name));
    obj.Set("name", jsName);
    obj.Set("type", Number::New(env, (uint32_t)valueType));

    // Handle different types of values
    if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
      data[dataLength] = 0;
      data[dataLength + 1] = 0;
      obj.Set("value", String::New(env, reinterpret_cast<char16_t*>(data)));
    }
    if (valueType == REG_DWORD) {
      obj.Set("value", Number::New(env, *(uint32_t*)&data[0]));
    }
    if (valueType == REG_BINARY) {
      auto val = Array::New(env);
      for (DWORD i = 0; i < dataLength; i++) {
        val.Set(i, Number::New(env, (uint32_t)data[i]));
      }
      obj.Set("value", val);
    }
    if (valueType == REG_MULTI_SZ) {
      auto val = Array::New(env);
      data[dataLength] = 0;
      data[dataLength + 1] = 0;
      DWORD pos = 0;
      DWORD idx = 0;
      while ( pos < (dataLength-2)) { // -2 because of the trailing null-wchar
        auto entry = String::New(env,reinterpret_cast<char16_t*>(data+pos));
        val.Set(idx++,  entry);
        pos += (entry.Utf16Value().length()+1) * 2 ;
      }

      obj.Set("value", val);
    }
    ret.Set(index++, obj);
  }

  if (key) {
    RegCloseKey(key);
  }

  return ret;
}

// Function to set a value of a registry key
Napi::Value setValue(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  auto key = openKey(info, KEY_WRITE);
  if (!key) {
    return env.Null();
  }

  auto valueType = (DWORD)(int64_t)info[2].ToNumber();
  DWORD dataLength = 0;

  // Handle different types of values
  if (valueType == REG_SZ || valueType == REG_EXPAND_SZ) {
    auto value = info[4].ToString().Utf16Value();
    wcscpy((wchar_t*)data, (LPCWSTR)value.c_str());
    dataLength = value.length() * 2 + 2;
    data[dataLength] = 0;
  }
  if (valueType == REG_DWORD) {
    *((DWORD*)&data) = (DWORD)(int64_t)info[4].ToNumber();
    dataLength = 4;
  }
  if (valueType == REG_MULTI_SZ) {
    if (info[4].IsArray()){
      auto value = info[4].As<Napi::Array>();
      uint32_t len = value.Length();

      for (uint32_t idx = 0; idx < len; idx++)
      {
        Napi::Value entry = value[idx];
        auto entryValue = entry.ToString().Utf16Value();
        wcscpy((wchar_t*)(&data[dataLength]), (LPCWSTR)entryValue.c_str());
        dataLength += entryValue.length() * 2 + 2;
      }
      // add the trailing null-wchar
      data[dataLength++] = 0;
      data[dataLength++] = 0;
    }
  }

  auto name = info[3].ToString().Utf16Value();

  LSTATUS error;
  if ((error = RegSetValueExW(key, (LPCWSTR)name.c_str(), NULL, valueType, (LPBYTE)&data, dataLength)) != ERROR_SUCCESS) {
    RegCloseKey(key);
    return Number::New(env, (uint32_t)error);
  }

  RegCloseKey(key);
  return env.Null();
}

// Function to list all subkeys of a registry key
Napi::Value listSubkeys(const Napi::CallbackInfo& info) {
  auto env = info.Env();
  auto key = openKey(info, KEY_ENUMERATE_SUB_KEYS);
  if (!key) {
    return env.Null();
  }

  DWORD index = 0;
  DWORD nameLength = VALUE_MAX_SIZE - 1;

  auto ret = Array::New(env);

  LSTATUS error;

  // Loop through all subkeys
  while (TRUE) {
    if ((error = RegEnumKeyW(key, index, (LPWSTR)&name, nameLength)) != ERROR_SUCCESS) {
      if (error == ERROR_NO_MORE_ITEMS) {
        break;
      }
      RegCloseKey(key);
      return Number::New(env, (uint32_t)error);
    }

    ret[index++] = String::New(env, reinterpret_cast<char16_t*>(name));
  }

  RegCloseKey(key);
  return ret;
}

// Function to create a registry key
Napi::Value createKey(const Napi::CallbackInfo& info) {
  HKEY key = 0;
  auto root = (HKEY)(int64_t)info[0].ToNumber();
  auto path = (LPCWSTR)info[1].ToString().Utf16Value().c_str();

  LSTATUS error;
  if ((error = RegCreateKeyExW(root, path, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &key, NULL)) != ERROR_SUCCESS) {
    return Number::New(info.Env(), error);
  }

  if (key) {
    RegCloseKey(key);
  }
  return info.Env().Null();
}

// Function to delete a registry key
Napi::Value deleteKey(const Napi::CallbackInfo& info) {
  auto root = (HKEY)(int64_t)info[0].ToNumber();
  auto path = (LPCWSTR)info[1].ToString().Utf16Value().c_str();
  RegDeleteTreeW(root, path);
  RegDeleteKeyW(root, path);
  return info.Env().Null();
}




Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("getWorkingDirectoryFromPID", Napi::Function::New(env, getWorkingDirectoryFromPID));
  exports.Set("getWorkingDirectoryFromHandle", Napi::Function::New(env, getWorkingDirectoryFromHandle));
  exports.Set("getKey", Napi::Function::New(env, getKey));
  exports.Set("setValue", Napi::Function::New(env, setValue));
  exports.Set("listSubkeys", Napi::Function::New(env, listSubkeys));
  exports.Set("createKey", Napi::Function::New(env, createKey));
  exports.Set("deleteKey", Napi::Function::New(env, deleteKey));
  return exports;
}

NODE_API_MODULE(addon, Init)