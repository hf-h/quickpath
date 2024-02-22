#define _Win32

#include "windows.h"
#include "ShlObj_core.h"
#include "stdio.h"
#include "cutils/UtMath.c"
#include "cutils/UtAlloc.c"
#include "cutils/UtString.c"
#include "cutils/UtWideString.c"

void Useage() {
    char *header = "Quick Link\n\nA way to \"tag\" paths and use them with other cli tools";
    char *addCommand = "add [tag name] adds the current path of the active shell with the specified tag to the qpcmds file";
    char *delCommand = "del [tag name] removes the specified tag and connected path from the qpcmds file";
    char *listCommand = "list lists all custom commands";
    char *customCommand = "[tag name] prints the path connected to the specified tag to stdout";
    printf("%s\n\nCommands:\n\n%s\n\n%s\n\n%s\n\n%s\n", header, addCommand, delCommand, listCommand, customCommand);
}

typedef struct {
    char *key;
    char *val;
} Command;

char *CommandsToString(AL *al, AL *tmpAl, Command *cmds, usize cmdC) {
    char **strBuilder = Alloc(tmpAl, sizeof(char *) * cmdC);
    for (usize i = 0; i < cmdC; i++) {
        strBuilder[i] = FormatString(tmpAl, "%s,%s\n", cmds[i].key, cmds[i].val);
    }

    return MergeStrs(al, strBuilder, cmdC);
}

void *ReadCommandFile(AL *al, HANDLE fh) {
    LARGE_INTEGER fileSize;
    if (FALSE == GetFileSizeEx(fh, &fileSize)) {
        printf("Err get file size: %lu\n", GetLastError());
        return NULL;
    }

    void *fileData = StrMake(al, fileSize.QuadPart);

    DWORD bytesRead = 0;
    if (!ReadFile(fh, fileData, fileSize.QuadPart, &bytesRead, NULL)) {
        return NULL;
    }

    return fileData;
}
void WriteCommandFile(HANDLE fh, char *contents) {
    SetFilePointer(fh, 0, NULL, FILE_BEGIN);
    SetEndOfFile(fh);

    DWORD bwC = 0;
    WriteFile(fh, contents, StrLen(contents), &bwC, NULL);
}

Command *ParseCommandFile(AL *al, char *fileData, usize *nrOfCommandsRead) {
    usize lineCount = 0;
    char **lines = SplitStr(al, fileData, '\n', &lineCount);

    if (lineCount > 0) {
        lineCount -= 1;
    }

    *nrOfCommandsRead = lineCount;
    Command *cmds = Alloc(al, sizeof(Command) * lineCount);
    for (usize i = 0; i < lineCount; i++) {
        usize _= 0;
        char **kw = SplitStr(al, lines[i], ',', &_);
        cmds[i].key = kw[0];
        cmds[i].val = kw[1];
    }

    return cmds;
}

void DeleteCommand(char *targetCmd, Command *cmds, usize cmdC) {
    for (usize i = 0; i < cmdC; i++) {
        if (StrEq(targetCmd, cmds[i].key)) {
            for (usize j = i; j < cmdC -1; j++) {
                cmds[j] = cmds[j + 1];
            }

            break;
        }
    }
}

HANDLE OpenCommandFile(AL *al) {
    WCHAR *pathBuilder[2] = {NULL, L"\\quickpath"};
    SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &pathBuilder[0]);
    WCHAR *dirPath = MergeStrsW(al, pathBuilder, 2);

    if (ERROR_PATH_NOT_FOUND == CreateDirectoryW(dirPath, NULL)) {
        return INVALID_HANDLE_VALUE;
    }

    WCHAR *fullPathBuilder[2] = {dirPath, L"\\qpcmds"};
    WCHAR *fullPath = MergeStrsW(al, fullPathBuilder, 2);

    HANDLE fh = CreateFileW(fullPath, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    CoTaskMemFree(pathBuilder[0]);

    return fh;
}

#define COMMAND 1
#define COMMAND_TARGET 2
int main(int argc, char **argv) {
    AL sysAlloc = AlMakeWin32Alloc();
    AL scratchBuffer = AlMakeScratchBuffer(&sysAlloc, 10000);
    AL memArena = AlMakeArena(&sysAlloc, 10000);

    if (argc < 2) {
        Useage();
        return 0;
    }

    HANDLE fh = OpenCommandFile(&scratchBuffer);
    if (INVALID_HANDLE_VALUE == fh) {
        printf("Err: %lu\n", GetLastError());
        return 1;
    }

    char *cmdFileData = ReadCommandFile(&sysAlloc, fh);
    if (NULL == cmdFileData) {
        cmdFileData = "";
    }

    if (StrEq("list", argv[COMMAND])) {
        printf("%s", cmdFileData);
        goto CLOSE;
    }

    if(StrEq("add", argv[COMMAND])) {
        char *cwd = Alloc(&memArena, MAX_PATH);
        GetCurrentDirectoryA(MAX_PATH, cwd);

        char *outStr = FormatString(&scratchBuffer, "%s%s,%s\n", cmdFileData, argv[COMMAND_TARGET], cwd);
        WriteCommandFile(fh, outStr);

        goto CLOSE;
    }

    usize cmdC = 0;
    Command *cmds = ParseCommandFile(&memArena, cmdFileData, &cmdC);
    if (StrEq("del", argv[COMMAND])) {
        if (argc < 3) {
            Useage();
            return 0;
        }

        DeleteCommand(argv[COMMAND_TARGET], cmds, cmdC);
        WriteCommandFile(fh, CommandsToString(&memArena, &scratchBuffer, cmds, cmdC -1));

        goto CLOSE;
    }

    for (usize i = 0; i < cmdC; i++) {
        if (StrEq(argv[COMMAND], cmds[i].key)) {
            printf("%s\n", cmds[i].val);
            goto CLOSE;
        }
    }

    Useage();

CLOSE:
    CloseHandle(fh);
    return 0;
}
















