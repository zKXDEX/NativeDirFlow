export interface RegistryValue {
    name: string;
    type: REG;
    value: any;
}
export declare enum HK {
    CR = 2147483648,
    CU = 2147483649,
    LM = 2147483650,
    U = 2147483651,
    PD = 2147483652,
    CC = 2147483653,
    DD = 2147483654
}
export declare enum REG {
    SZ = 1,
    EXPAND_SZ = 2,
    BINARY = 3,
    DWORD = 4,
    DWORD_BIG_ENDIAN = 5,
    DWORD_LITTLE_ENDIAN = 4,
    LINK = 6,
    MULTI_SZ = 7,
    RESOURCE_LIST = 8
}
export declare function getRegistryKey(root: HK, path: string): {
    [name: string]: RegistryValue;
} | null;
export declare function getRegistryValue(root: HK, path: string, name: string): any;
export declare function setRegistryValue(root: HK, path: string, name: string, type: REG.MULTI_SZ, value: string[]): any;
export declare function setRegistryValue(root: HK, path: string, name: string, type: REG, value: string): any;
export declare function listRegistrySubkeys(root: HK, path: string): string[];
export declare function createRegistryKey(root: HK, path: string): any;
export declare function deleteRegistryKey(root: HK, path: string): any;
export declare function getWorkingDirectoryFromPID(pid: number): string | null;
export declare function getWorkingDirectoryFromHandle(handle: number): string | null;
