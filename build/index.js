"use strict";
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    var desc = Object.getOwnPropertyDescriptor(m, k);
    if (!desc || ("get" in desc ? !m.__esModule : desc.writable || desc.configurable)) {
      desc = { enumerable: true, get: function() { return m[k]; } };
    }
    Object.defineProperty(o, k2, desc);
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
Object.defineProperty(exports, "__esModule", { value: true });
exports.getWorkingDirectoryFromHandle = exports.getWorkingDirectoryFromPID = exports.deleteRegistryKey = exports.createRegistryKey = exports.listRegistrySubkeys = exports.setRegistryValue = exports.getRegistryValue = exports.getRegistryKey = exports.REG = exports.HK = void 0;
var fs = __importStar(require("fs"));
var HK;
(function (HK) {
    HK[HK["CR"] = 2147483648] = "CR";
    HK[HK["CU"] = 2147483649] = "CU";
    HK[HK["LM"] = 2147483650] = "LM";
    HK[HK["U"] = 2147483651] = "U";
    HK[HK["PD"] = 2147483652] = "PD";
    HK[HK["CC"] = 2147483653] = "CC";
    HK[HK["DD"] = 2147483654] = "DD"; // Current dynamic data
})(HK = exports.HK || (exports.HK = {}));
var native;
function getNative() {
    if (!native) {
        native = require('./build/Release/NativeScopeKit.node');
    }
    return native;
}
// Define the possible types of a registry value
var REG;
(function (REG) {
    REG[REG["SZ"] = 1] = "SZ";
    REG[REG["EXPAND_SZ"] = 2] = "EXPAND_SZ";
    REG[REG["BINARY"] = 3] = "BINARY";
    REG[REG["DWORD"] = 4] = "DWORD";
    REG[REG["DWORD_BIG_ENDIAN"] = 5] = "DWORD_BIG_ENDIAN";
    REG[REG["DWORD_LITTLE_ENDIAN"] = 4] = "DWORD_LITTLE_ENDIAN";
    REG[REG["LINK"] = 6] = "LINK";
    REG[REG["MULTI_SZ"] = 7] = "MULTI_SZ";
    REG[REG["RESOURCE_LIST"] = 8] = "RESOURCE_LIST";
})(REG = exports.REG || (exports.REG = {}));
// Function to get a registry key and its values
function getRegistryKey(root, path) {
    var ret = {};
    var key = getNative().getKey(root, path);
    if (!key) {
        return null;
    }
    for (var _i = 0, key_1 = key; _i < key_1.length; _i++) {
        var value = key_1[_i];
        ret[value.name] = value;
    }
    return ret;
}
exports.getRegistryKey = getRegistryKey;
// Function to get a specific value of a registry key
function getRegistryValue(root, path, name) {
    var key = getRegistryKey(root, path);
    if (!key || !key[name]) {
        return null;
    }
    return key[name].value;
}
exports.getRegistryValue = getRegistryValue;
function cleanupPath(path) {
    if (!path) {
        return null;
    }
    if (path.charCodeAt(path.length - 1) < 32) {
        path = path.substring(0, path.length - 1);
    }
    return path;
}
function setRegistryValue(root, path, name, type, value) {
    return getNative().setValue(root, path, type, name, value);
}
exports.setRegistryValue = setRegistryValue;
function listRegistrySubkeys(root, path) {
    return getNative().listSubkeys(root, path);
}
exports.listRegistrySubkeys = listRegistrySubkeys;
function createRegistryKey(root, path) {
    return getNative().createKey(root, path);
}
exports.createRegistryKey = createRegistryKey;
function deleteRegistryKey(root, path) {
    return getNative().deleteKey(root, path);
}
exports.deleteRegistryKey = deleteRegistryKey;
// Directory functions
function getWorkingDirectoryFromPID(pid) {
    if (process.platform === 'linux') {
        return fs.readlinkSync("/proc/".concat(pid, "/cwd"));
    }
    return cleanupPath(getNative().getWorkingDirectoryFromPID(pid));
}
exports.getWorkingDirectoryFromPID = getWorkingDirectoryFromPID;
function getWorkingDirectoryFromHandle(handle) {
    if (process.platform !== 'win32') {
        throw new Error('getWorkingDirectoryFromHandle() is only available on Windows');
    }
    return cleanupPath(getNative().getWorkingDirectoryFromHandle(handle));
}
exports.getWorkingDirectoryFromHandle = getWorkingDirectoryFromHandle;
