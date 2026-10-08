/*
 * SISTEMAS OPERATIVOS - PRÁCTICA 1 (2026/2027)
 * Textos de ayuda de los comandos del shell
 */

#ifndef AYUDA_H
#define AYUDA_H

typedef struct {
    const char *nombre;
    const char *uso;
    const char *descripcion;
} Ayuda;

static const Ayuda AYUDAS[] = {
    {"authors", "authors [-l|-n]",
     "shows the shell's authors (names and logins)\n"
     "\t-l: shows only the logins\n"
     "\t-n: shows only the names"},
    {"pid", "pid [-p]",
     "shows ths shell's pid\n"
     "\t-p: shows the pid of the shell's parent process"},
    {"date", "date [-d|-t]",
     "shows the current date and time.\n"
     "\t-d: shows only the date (DD/MM/AAAA)\n"
     "\t-t: shows only the time (hh:mm:ss)"},
    {"sysinfo", "sysinfo",
     "shows information about the machine running the shell (like uname -a)"},
    {"help", "help [cmd]",
     "lists the available commands. help cmd shows help for the cmd command"},
    {"exit", "exit", "terminates the shell's execution"},
    {"bye", "bye", "terminates the shell's execution"},
    {"chdir", "chdir [dir]",
     "changes the shell's working directory to dir.\n"
     "Without arguments it shows the current working directory"},
    {"open", "open [fich] [m1] [m2] ...",
     "opens the file fich and adds it to the list of open files in the shell.\n"
     "Modes: cr: O_CREAT, ap: O_APPEND, ex: O_EXCL, ro: O_RDONLY,\n"
     "       rw: O_RDWR, wo: O_WRONLY, tr: O_TRUNC\n"
     "Without arguments it lists the open files in the shell"},
    {"close", "close df [-f]",
     "closes the file descriptor df, and eliminates the corresponding list entry\n"
     "\t-f: closes even if df corresponds to an active mapping"},
    {"listopen", "listopen",
     "lists the open files in the shell (descriptor, name and mode)"},
    {"dup", "dup df",
     "duplicates the file descriptor df and adds the new descriptor to the list of open files"},
    {"lseek", "lseek df pos ref",
     "positions the offset if file descriptor to pos. ref is the reference and can be:\n"
     "\tSEEK_SET: pos is relative to the beginning of the file\n"
     "\tSEEK_CUR: pos is relative to the current position\n"
     "\tSEEK_END: pos is relative to the end of the file"},
    {"readstr", "readstr df cont",
     "reads cont bytes from the file descriptor df and displays them on screen as a string"},
    {"writestr", "writestr df str",
     "writes the string str to the file opened with descriptor df"},
    {"makefile", "makefile nombre",
     "creates an empty file with the name nombre"},
    {"makedir", "makedir nombre",
     "creates a directory with the name nombre"},
    {"delete", "delete n1 n2 ...",
     "deletes the files, links and/or empty directories n1, n2 ..."},
    {"deltree", "deltree n1 n2 ...",
     "deletes the files, links and/or directories n1, n2 ...\n"
     "If a directory is not empty, it is deleted along with all its content"},
    {"listfile", "listfile [-long] [-link] [-acc] n1 n2 ...",
     "Displays information about the file system objects n1, n2 ...\n"
     "If a name is a directory, information about the directory itself is displayed.\n"
     "By default, only the name and size are shown.\n"
     "\t-long: long listing (date, links, inode, owner, group, mode)\n"
     "\t-link: if it is a symbolic link, also shows where it points\n"
     "\t-acc:  uses the last access date"},
    {"list", "list [-reca] [-recb] [-hid] [-long] [-link] [-acc] n1 n2 ...",
     "Similar to listfile, but if a name is a directory, its content is listed.\n"
     "\t-hid:  lists also the hidden files\n"
     "\t-reca: recursive; the recursion is done AFTER listing the directory\n"
     "\t-recb: recursive; the recursion is done BEFORE listing the directory\n"
     "\t-long, -link, -acc: as in listfile"},
};

#define NUM_AYUDAS ((int)(sizeof(AYUDAS) / sizeof(AYUDAS[0])))

#endif
