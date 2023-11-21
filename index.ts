import * as fs from 'fs'

let native;

function getNative(){
    if(!native){
        native = require('./build/Release/NativeDirFlow.node')
    }
    return native
}

function cleanUpPath(path: string){
    if (!path){
        return null
    }

    if (path.charCodeAt(path.length - 1) < 32){
        path = path.substring(0, path.length - 1)
    }
    return path
}

export function getWorkingDirectoryFromPID(pid: number) : string|null {
    if (process.platform == "linux"){
        return fs.readlinkSync('/proc/${pid}/cwd')
    }   
    return cleanUpPath(getNative().getWorkingDirectoryFromPID(pid))
}

export function getWorkingDirectHandle(handle: number): string|null{
    if (process.platform !== 'win32') {
        throw new Error('getWorkingDirectoryFromHandle() is only available on Windows')
      }
    return cleanUpPath(getNative().getWorkingDirectHandle(handle))
}

