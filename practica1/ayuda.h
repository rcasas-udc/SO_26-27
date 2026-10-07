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
     "Muestra los nombres y logins de los autores del shell.\n"
     "\t-l: muestra solo los logins\n"
     "\t-n: muestra solo los nombres"},
    {"pid", "pid [-p]",
     "Muestra el pid del shell.\n"
     "\t-p: muestra el pid del proceso padre del shell"},
    {"date", "date [-d|-t]",
     "Muestra la fecha y la hora actuales.\n"
     "\t-d: muestra solo la fecha (DD/MM/AAAA)\n"
     "\t-t: muestra solo la hora (hh:mm:ss)"},
    {"sysinfo", "sysinfo",
     "Muestra informacion de la maquina que ejecuta el shell (como uname -a)"},
    {"help", "help [cmd]",
     "Lista los comandos disponibles. help cmd muestra la ayuda del comando cmd"},
    {"exit", "exit", "Termina la ejecucion del shell"},
    {"bye", "bye", "Termina la ejecucion del shell"},
    {"chdir", "chdir [dir]",
     "Cambia el directorio de trabajo del shell a dir.\n"
     "Sin argumentos muestra el directorio de trabajo actual"},
    {"open", "open [fich] [m1] [m2] ...",
     "Abre el fichero fich y lo anade a la lista de ficheros abiertos del shell.\n"
     "Modos: cr: O_CREAT, ap: O_APPEND, ex: O_EXCL, ro: O_RDONLY,\n"
     "       rw: O_RDWR, wo: O_WRONLY, tr: O_TRUNC\n"
     "Sin argumentos lista los ficheros abiertos del shell"},
    {"close", "close df [-f]",
     "Cierra el descriptor df y elimina su entrada de la lista de ficheros abiertos.\n"
     "\t-f: cierra aunque df corresponda a un mapeo activo"},
    {"listopen", "listopen",
     "Lista los ficheros abiertos del shell (descriptor, nombre y modo)"},
    {"dup", "dup df",
     "Duplica el descriptor df y anade el nuevo descriptor a la lista de ficheros abiertos"},
    {"lseek", "lseek df pos ref",
     "Posiciona el offset del descriptor df en pos. ref puede ser:\n"
     "\tSEEK_SET: pos relativo al principio del fichero\n"
     "\tSEEK_CUR: pos relativo a la posicion actual\n"
     "\tSEEK_END: pos relativo al final del fichero"},
    {"readstr", "readstr df cont",
     "Lee cont bytes del descriptor df y los muestra en pantalla como una cadena"},
    {"writestr", "writestr df str",
     "Escribe la cadena str en el fichero abierto con descriptor df"},
    {"makefile", "makefile nombre",
     "Crea un fichero vacio de nombre nombre"},
    {"makedir", "makedir nombre",
     "Crea un directorio de nombre nombre"},
    {"delete", "delete n1 n2 ...",
     "Borra los ficheros, enlaces y/o directorios vacios n1, n2 ..."},
    {"deltree", "deltree n1 n2 ...",
     "Borra los ficheros, enlaces y/o directorios n1, n2 ...\n"
     "Si un directorio no esta vacio se borra junto con todo su contenido"},
    {"listfile", "listfile [-long] [-link] [-acc] n1 n2 ...",
     "Muestra informacion de los objetos del sistema de ficheros n1, n2 ...\n"
     "Si un nombre es un directorio, se muestra informacion del propio directorio.\n"
     "Por defecto solo se muestran nombre y tamano.\n"
     "\t-long: listado largo (fecha, enlaces, inodo, propietario, grupo, modo)\n"
     "\t-link: si es un enlace simbolico, muestra tambien a donde apunta\n"
     "\t-acc:  usa la fecha de ultimo acceso"},
    {"list", "list [-reca] [-recb] [-hid] [-long] [-link] [-acc] n1 n2 ...",
     "Como listfile, pero si un nombre es un directorio se lista su contenido.\n"
     "\t-hid:  lista tambien los ficheros ocultos\n"
     "\t-reca: recursivo; la recursion se hace DESPUES de listar el directorio\n"
     "\t-recb: recursivo; la recursion se hace ANTES de listar el directorio\n"
     "\t-long, -link, -acc: como en listfile"},
};

#define NUM_AYUDAS ((int)(sizeof(AYUDAS) / sizeof(AYUDAS[0])))

#endif
