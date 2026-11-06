/*
 * Operating Systems - Lab Assignment 1
 *
 * Authors:
 *   Francisco Martínez Rubido    login: f.martinezr@udc.es
 *   Román Casas Riveira          login: r.casas@udc.es
 */

#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <ctype.h>

#define MAXENTRADA 2048

typedef enum{
    MALLOC_MEM, SHARED_MEM, MMAP_MEM
} memType;

typedef struct node{
    void *address;
    key_t key;
    size_t size;
    memType type;
    char *filename;
    int df;
    time_t time;
    struct node *prev;
    struct node *next;
} node;

typedef struct tList{
    node *head;
    node *tail;
}tList;

tList list;

struct fichero {
    int df;
    int modo;
    char *nombre;
    struct fichero *sig;
};

struct fichero *lista = NULL;


// inserts the file into the list ordered by df; if it was already there, updates it
int AnadirFichero(int df, int modo, char *nombre)
{
    struct fichero *ant = NULL, *p = lista, *nuevo;
    char *aux;

    while (p != NULL && p->df < df) {
        ant = p;
        p = p->sig;
    }

    if (p != NULL && p->df == df) {
        if ((aux = strdup(nombre)) == NULL)
            return -1;
        free(p->nombre);
        p->nombre = aux;
        p->modo = modo;
        return 0;
    }

    nuevo = malloc(sizeof(struct fichero));
    if (nuevo == NULL)
        return -1;
    nuevo->nombre = strdup(nombre);
    if (nuevo->nombre == NULL) {
        free(nuevo);
        return -1;
    }
    nuevo->df = df;
    nuevo->modo = modo;
    nuevo->sig = p;

    if (ant == NULL)
        lista = nuevo;
    else
        ant->sig = nuevo;
    return 0;
}

struct fichero *BuscarFichero(int df)
{
    struct fichero *p;

    for (p = lista; p != NULL; p = p->sig)
        if (p->df == df)
            return p;
    return NULL;
}

void EliminarFichero(int df)
{
    struct fichero *ant = NULL, *p = lista;

    while (p != NULL && p->df != df) {
        ant = p;
        p = p->sig;
    }
    if (p == NULL)
        return;

    if (ant == NULL)
        lista = p->sig;
    else
        ant->sig = p->sig;
    free(p->nombre);
    free(p);
}

void BorrarLista()
{
    struct fichero *aux;

    while (lista != NULL) {
        aux = lista;
        lista = lista->sig;
        free(aux->nombre);
        free(aux);
    }
}

void ModoATexto(int modo, char *texto)
{
    int acc = modo & O_ACCMODE;

    if (acc == O_RDONLY)
        strcpy(texto, "O_RDONLY");
    else if (acc == O_WRONLY)
        strcpy(texto, "O_WRONLY");
    else if (acc == O_RDWR)
        strcpy(texto, "O_RDWR");
    else
        strcpy(texto, "O_???");

    if (modo & O_CREAT)    strcat(texto, " O_CREAT");
    if (modo & O_EXCL)     strcat(texto, " O_EXCL");
    if (modo & O_TRUNC)    strcat(texto, " O_TRUNC");
    if (modo & O_APPEND)   strcat(texto, " O_APPEND");
    if (modo & O_NONBLOCK) strcat(texto, " O_NONBLOCK");
}

// gets the real name from /proc; if that is not possible, uses a generic one
void NombreDescriptor(int df, char *nombre, int tam)
{
    char ruta[64];
    ssize_t n;

    sprintf(ruta, "/proc/self/fd/%d", df);
    n = readlink(ruta, nombre, tam - 1);
    if (n != -1) {
        nombre[n] = '\0';
        return;
    }

    if (df == 0)
        strcpy(nombre, "standard input");
    else if (df == 1)
        strcpy(nombre, "standard output");
    else if (df == 2)
        strcpy(nombre, "standard error");
    else
        snprintf(nombre, tam, "inherited descriptor %d", df);
}

void CargarHeredados()
{
    char nombre[PATH_MAX];
    int df, modo;
    long max = sysconf(_SC_OPEN_MAX);

    if (max < 0 || max > 65536)
        max = 65536;

    for (df = 0; df < max; df++) {
        modo = fcntl(df, F_GETFL);
        if (modo == -1)
            continue;   // not open
        NombreDescriptor(df, nombre, sizeof(nombre));
        AnadirFichero(df, modo, nombre);
    }
}

void ListarAbiertos()
{
    struct fichero *p;
    char texto[128];
    int modo;

    for (p = lista; p != NULL; p = p->sig) {
        // the current mode is requested from fcntl so it matches lsof,
        // but O_CREAT, O_EXCL and O_TRUNC are only known from when it was opened
        modo = fcntl(p->df, F_GETFL);
        if (modo == -1)
            modo = p->modo;
        modo |= p->modo & (O_CREAT | O_EXCL | O_TRUNC);

        ModoATexto(modo, texto);
        printf("descriptor: %2d -> %-30s %s\n", p->df, p->nombre, texto);
    }
}

// for system call errors, objeto can be NULL
void ErrorSis(const char *accion, const char *objeto)
{
    int err = errno;

    if (objeto != NULL)
        printf("Cannot %s %s: %s\n", accion, objeto, strerror(err));
    else
        printf("Cannot %s: %s\n", accion, strerror(err));
}

// converts the string to a descriptor, returns -1 if it is not a valid number
int LeerDf(char *s, int *df)
{
    char *fin;
    long v;

    errno = 0;
    v = strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || v < 0 || v > INT_MAX)
        return -1;
    *df = (int) v;
    return 0;
}

int NumArgs(char *arg[])
{
    int i = 0;

    while (arg[i] != NULL)
        i++;
    return i;
}


// the path is allocated with malloc so there is no length limit during recursion
char *UnirRuta(const char *dir, const char *nombre)
{
    size_t lon = strlen(dir) + strlen(nombre) + 2;
    char *ruta = malloc(lon);

    if (ruta != NULL)
        snprintf(ruta, lon, "%s/%s", dir, nombre);
    return ruta;
}

/*************SHELL COMMANDS************************/
/* all of them receive the pieces without the command name: arg[0] is the first argument */

void Cmd_fin(char *arg[])
{
    BorrarLista();
    exit(0);
}

void Cmd_authors(char *arg[])
{
    char *nombres[] = {"Francisco Martínez Rubido", "Román Casas Riveira"};
    char *logins[] = {"f.martinezr@udc.es", "r.casas@udc.es"};
    int nom = 1, log = 1, i;

    if (arg[0] != NULL) {
        if (!strcmp(arg[0], "-l"))
            nom = 0;
        else if (!strcmp(arg[0], "-n"))
            log = 0;
        else {
            printf("Invalid option: %s\n", arg[0]);
            return;
        }
    }

    for (i = 0; i < 2; i++) {
        if (nom && log)
            printf("%s: %s\n", nombres[i], logins[i]);
        else if (nom)
            printf("%s\n", nombres[i]);
        else
            printf("%s\n", logins[i]);
    }
}

void Cmd_date(char *arg[])
{
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char buf[32];
    int fecha = 1, hora = 1;

    if (arg[0] != NULL) {
        if (!strcmp(arg[0], "-d"))
            hora = 0;
        else if (!strcmp(arg[0], "-t"))
            fecha = 0;
        else {
            printf("Invalid option: %s\n", arg[0]);
            return;
        }
    }
    if (t == NULL) {
        printf("Cannot get the date\n");
        return;
    }

    if (fecha) {
        strftime(buf, sizeof(buf), "%d/%m/%Y", t);
        printf("%s\n", buf);
    }
    if (hora) {
        strftime(buf, sizeof(buf), "%H:%M:%S", t);
        printf("%s\n", buf);
    }
}

void Cmd_open(char *arg[])
{
    int i, df, modo = 0;

    if (arg[0] == NULL) {
        ListarAbiertos();
        return;
    }

    for (i = 1; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "cr"))
            modo |= O_CREAT;
        else if (!strcmp(arg[i], "ex"))
            modo |= O_EXCL;
        else if (!strcmp(arg[i], "ro"))
            modo |= O_RDONLY;
        else if (!strcmp(arg[i], "wo"))
            modo |= O_WRONLY;
        else if (!strcmp(arg[i], "rw"))
            modo |= O_RDWR;
        else if (!strcmp(arg[i], "ap"))
            modo |= O_APPEND;
        else if (!strcmp(arg[i], "tr"))
            modo |= O_TRUNC;
        else {
            printf("Invalid mode: %s\n", arg[i]);
            return;
        }
    }

    df = open(arg[0], modo, 0777);
    if (df == -1) {
        ErrorSis("open", arg[0]);
        return;
    }
    if (AnadirFichero(df, modo, arg[0]) == -1) {
        printf("Cannot add %s to the list: not enough memory\n", arg[0]);
        close(df);
        return;
    }
    printf("Added entry %d to the open files table\n", df);
}

void Cmd_close(char *arg[])
{
    int i, df, forzar = 0;
    char *num = NULL;
    node *m;

    for (i = 0; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "-f")) {
            forzar = 1;
            continue;
        }
        if (num != NULL || LeerDf(arg[i], &df) == -1) {
            printf("Usage: close df [-f]\n");
            return;
        }
        num = arg[i];
    }
    if (num == NULL) {
        ListarAbiertos();
        return;
    }

    // a mapped file keeps its descriptor, it is only closed with -f
    for (m = list.head; m != NULL; m = m->next)
        if (m->type == MMAP_MEM && m->df == df)
            break;
    if (m != NULL && !forzar) {
        printf("Descriptor %d is used by the mapping of %s, use -f to close it anyway\n",
               df, m->filename);
        return;
    }

    fflush(stdout);   // in case we close the standard output
    if (close(df) == -1) {
        ErrorSis("close descriptor", num);
        return;
    }
    EliminarFichero(df);
    if (m != NULL)
        m->df = -1;   // so deallocate does not close it again
}

void Cmd_dup(char *arg[])
{
    int df, nuevo, modo;
    char nombre[PATH_MAX];
    struct fichero *orig;

    if (arg[0] == NULL) {
        ListarAbiertos();
        return;
    }

    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }

    fflush(stdout);
    if ((nuevo = dup(df)) == -1) {
        ErrorSis("duplicate descriptor", arg[0]);
        return;
    }

    orig = BuscarFichero(df);
    if (orig != NULL)
        strcpy(nombre, orig->nombre);
    else
        NombreDescriptor(df, nombre, sizeof(nombre));

    modo = fcntl(nuevo, F_GETFL);
    if (orig != NULL)
        modo |= orig->modo & (O_CREAT | O_EXCL | O_TRUNC);

    if (AnadirFichero(nuevo, modo, nombre) == -1) {
        printf("Cannot add the duplicate to the list: not enough memory\n");
        close(nuevo);
        return;
    }
    printf("Added entry %d to the open files table\n", nuevo);
}

void Cmd_lseek(char *arg[])
{
    int df, ref;
    off_t pos, res;
    char *fin;

    if (NumArgs(arg) != 3) {
        printf("Usage: lseek df pos SEEK_SET|SEEK_CUR|SEEK_END\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }

    errno = 0;
    pos = strtoll(arg[1], &fin, 10);
    if (errno != 0 || *fin != '\0') {
        printf("Invalid position: %s\n", arg[1]);
        return;
    }

    if (!strcmp(arg[2], "SEEK_SET"))
        ref = SEEK_SET;
    else if (!strcmp(arg[2], "SEEK_CUR"))
        ref = SEEK_CUR;
    else if (!strcmp(arg[2], "SEEK_END"))
        ref = SEEK_END;
    else {
        printf("Invalid reference: %s (use SEEK_SET, SEEK_CUR or SEEK_END)\n", arg[2]);
        return;
    }

    res = lseek(df, pos, ref);
    if (res == -1)
        ErrorSis("seek descriptor", arg[0]);
    else
        printf("New offset: %lld\n", (long long) res);
}

void Cmd_readstr(char *arg[])
{
    int df;
    long cont;
    ssize_t leidos;
    char *fin, *buf;

    if (NumArgs(arg) != 2) {
        printf("Usage: readstr df cont\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }

    errno = 0;
    cont = strtol(arg[1], &fin, 10);
    if (errno != 0 || *fin != '\0' || cont < 0 || cont >= SSIZE_MAX) {
        printf("Invalid number of bytes: %s\n", arg[1]);
        return;
    }

    buf = malloc(cont + 1);   // +1 for the '\0'
    if (buf == NULL) {
        printf("Cannot allocate %ld bytes\n", cont);
        return;
    }

    leidos = read(df, buf, cont);
    if (leidos == -1) {
        ErrorSis("read from descriptor", arg[0]);
        free(buf);
        return;
    }
    buf[leidos] = '\0';
    printf("%s\n", buf);
    printf("Read %ld bytes from descriptor %d\n", (long) leidos, df);
    free(buf);
}

void Cmd_writestr(char *arg[])
{
    int df, i, tam = 0;
    ssize_t escritos;
    char *cad;

    if (NumArgs(arg) < 2) {
        printf("Usage: writestr df str\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }

    // strtok splits on spaces, so the pieces have to be joined again
    for (i = 1; arg[i] != NULL; i++)
        tam += strlen(arg[i]) + 1;
    cad = malloc(tam);
    if (cad == NULL) {
        printf("Cannot allocate memory\n");
        return;
    }
    strcpy(cad, arg[1]);
    for (i = 2; arg[i] != NULL; i++) {
        strcat(cad, " ");
        strcat(cad, arg[i]);
    }

    escritos = write(df, cad, strlen(cad));
    if (escritos == -1)
        ErrorSis("write to descriptor", arg[0]);
    else
        printf("Wrote %ld bytes to descriptor %d\n", (long) escritos, df);
    free(cad);
}

void Cmd_listopen(char * arg[]){
    ListarAbiertos();
}


void Cmd_sysinfo(char * arg[]){
    struct utsname s;
    if(uname(&s)==-1){
        perror("sysinfo");
    }
    else
        printf("%s %s %s %s %s\n", s.sysname, s.nodename, s.machine, s.release, s.version);
}

// help texts, one entry per command
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
    {"read", "read df addr cont",
     "transfers cont bytes from the open file df to the address addr (in hex)"},
    {"write", "write df addr cont",
     "transfers cont bytes from the address addr (in hex) to the open file df"},
    {"recurse", "recurse n",
     "executes the recursive function n times, showing the addresses of its\n"
     "automatic array, its static array and its parameter"},
    {"memfill", "memfill addr cont hcode",
     "fills cont bytes starting at address addr with the hex value hcode"},
    {"memdump", "memdump addr cont",
     "dumps onto screen cont bytes starting at address addr (characters and hex codes)"},
    {"allocate", "allocate [-malloc|-shared|-createshared|-mmap] ...",
     "allocates a block of memory. Without parameters shows the list of allocated blocks\n"
     "\t-malloc siz: allocates a block of size siz with malloc\n"
     "\t-createshared cl siz: creates a block of shared memory with key cl and size siz\n"
     "\t-shared cl: attaches the existing block of shared memory with key cl\n"
     "\t-mmap fil perm: maps the file fil in memory with permissions perm (r, w, x)"},
    {"deallocate", "deallocate [-malloc|-shared|-delkey|-mmap|addr] ...",
     "deallocates a block of memory. Without parameters shows the list of allocated blocks\n"
     "\t-malloc siz: frees a malloc block of size siz\n"
     "\t-shared cl: detaches the block of shared memory with key cl\n"
     "\t-mmap fil: unmaps the file fil\n"
     "\t-delkey cl: removes the key cl from the system (does not detach anything)\n"
     "\taddr: deallocates the block at address addr"},
    {"i-o", "i-o [read|write] [-o] f addr cnt",
     "direct input output from memory to files\n"
     "\tread f addr cnt: reads cnt bytes from file f into address addr (in hex)\n"
     "\twrite [-o] f addr cnt: writes cnt bytes from address addr into file f\n"
     "\t-o: the file can be overwritten"},
    {"memory", "memory [-blocks|-funcs|-vars|-all|-pmap]",
     "shows information on the shell's memory\n"
     "\t-blocks: the list of allocated blocks\n"
     "\t-funcs: addresses of three shell functions and three library functions\n"
     "\t-vars: addresses of automatic, static and external variables\n"
     "\t-all: -blocks, -funcs and -vars\n"
     "\t-pmap: shows the shell's address space with pmap"},
};

#define NUM_AYUDAS ((int)(sizeof(AYUDAS) / sizeof(AYUDAS[0])))

void Cmd_help(char * arg[]){
    int i;

    if (arg[0] == NULL) {
        printf("Available commands:\n");
        for (i = 0; i < NUM_AYUDAS; i++)
            printf("%s ", AYUDAS[i].nombre);
        printf("\n");
        return;
    }
    for (i = 0; i < NUM_AYUDAS; i++)
        if (!strcmp(AYUDAS[i].nombre, arg[0])) {
            printf("%s\t%s\n", AYUDAS[i].uso, AYUDAS[i].descripcion);
            return;
        }
    printf("%s not found\n", arg[0]);
}

void Cmd_makedir(char * arg[]){
    int i;

    if (arg[0]==NULL){
        printf("Cannot create a directory, please insert a name\n");
        return;
    }
    for (i = 0; arg[i] != NULL; i++)
        if (mkdir(arg[i], 0755)!=0)
            ErrorSis("create directory", arg[i]);
}

// the type is checked with lstat, as the assignment requires, so links are not followed:
// that way a link to a directory is removed with unlink and the directory is left untouched
int BorrarObjeto(char *nombre)
{
    struct stat st;

    if (lstat(nombre, &st) == -1) {
        ErrorSis("access", nombre);
        return -1;
    }
    if (S_ISDIR(st.st_mode)) {
        if (rmdir(nombre) == -1) {
            ErrorSis("delete directory", nombre);
            return -1;
        }
    } else if (unlink(nombre) == -1) {
        ErrorSis("delete", nombre);
        return -1;
    }
    return 0;
}

void Cmd_delete(char * arg[]){
    int i;

    if (arg[0]==NULL){
        printf("Cannot delete, please insert a name\n");
        return;
    }
    for (i = 0; arg[i] != NULL; i++)
        BorrarObjeto(arg[i]);
}

void Cmd_makefile(char * arg[]){
    int df;

    if (arg[0]==NULL){
        printf("Cannot create a file, please insert a name\n");
        return;
    }
    df = open(arg[0], O_CREAT | O_WRONLY, 0777);
    if (df == -1)
        ErrorSis("create file", arg[0]);
    else
        close(df);   // if it stayed open it would not be in the list and would not match lsof
}

// full paths are used instead of chdir so that, if something
// fails halfway, the shell is not left in another directory
int BorrarArbol(char *nombre)
{
    struct stat st;
    DIR *d;
    struct dirent *ent;
    char *ruta;
    int res = 0;

    if (lstat(nombre, &st) == -1) {
        ErrorSis("access", nombre);
        return -1;
    }
    if (!S_ISDIR(st.st_mode))
        return BorrarObjeto(nombre);

    if ((d = opendir(nombre)) == NULL) {
        ErrorSis("open directory", nombre);
        return -1;
    }
    while ((ent = readdir(d)) != NULL) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        if ((ruta = UnirRuta(nombre, ent->d_name)) == NULL) {
            printf("Cannot allocate memory\n");
            res = -1;
            break;
        }
        if (BorrarArbol(ruta) == -1)
            res = -1;
        free(ruta);
    }
    closedir(d);

    // if it could not be emptied, rmdir would fail and repeat the error
    if (res == -1)
        return -1;
    return BorrarObjeto(nombre);
}

void Cmd_deltree(char * arg[]){
    int i;

    if (arg[0]==NULL){
        printf("Cannot delete, please insert a name\n");
        return;
    }
    for (i = 0; arg[i] != NULL; i++)
        BorrarArbol(arg[i]);
}

void Cmd_pid (char * arg[])
{
    if (arg[0]==NULL)
        printf ("Shell pid: %d\n",(int) getpid());
    else
        if (!strcmp (arg[0],"-p"))
            printf ("Shell parent pid: %d\n",(int) getppid());
        else
            printf ("Invalid option: %s\n", arg[0]);
}

void Cmd_chdir (char * arg[])
{
    char dir[PATH_MAX];
   if (arg[0]==NULL){
      if ((getcwd(dir, sizeof(dir)))!=NULL){
            printf("%s\n", dir);
    }
        else {
            ErrorSis("get current directory", NULL);
        }
   }
    else if (chdir(arg[0])==-1)
      ErrorSis("change to directory", arg[0]);
}

void MostrarDirActual()
{
    char dir[PATH_MAX];

    if (getcwd(dir, sizeof(dir)) == NULL)
        ErrorSis("get current directory", NULL);
    else
        printf("%s\n", dir);
}

#define REC_NO      0
#define REC_DESPUES 1
#define REC_ANTES   2

struct OPCIONES {
    int largo;
    int enlace;
    int acceso;
    int ocultos;
    int recursion;
};

char LetraTipo(mode_t m)
{
    if (S_ISREG(m))  return '-';
    if (S_ISDIR(m))  return 'd';
    if (S_ISLNK(m))  return 'l';
    if (S_ISCHR(m))  return 'c';
    if (S_ISBLK(m))  return 'b';
    if (S_ISFIFO(m)) return 'p';
    if (S_ISSOCK(m)) return 's';
    return '?';
}

void CadenaPermisos(mode_t m, char *p)
{
    p[0] = LetraTipo(m);
    p[1] = (m & S_IRUSR) ? 'r' : '-';
    p[2] = (m & S_IWUSR) ? 'w' : '-';
    p[3] = (m & S_IXUSR) ? 'x' : '-';
    p[4] = (m & S_IRGRP) ? 'r' : '-';
    p[5] = (m & S_IWGRP) ? 'w' : '-';
    p[6] = (m & S_IXGRP) ? 'x' : '-';
    p[7] = (m & S_IROTH) ? 'r' : '-';
    p[8] = (m & S_IWOTH) ? 'w' : '-';
    p[9] = (m & S_IXOTH) ? 'x' : '-';
    if (m & S_ISUID) p[3] = (m & S_IXUSR) ? 's' : 'S';
    if (m & S_ISGID) p[6] = (m & S_IXGRP) ? 's' : 'S';
    if (m & S_ISVTX) p[9] = (m & S_IXOTH) ? 't' : 'T';
    p[10] = '\0';
}

// path and name are received separately because when listing a directory
// lstat needs the full path but only the name is shown
void ImprimirInfo(const char *ruta, const char *nombre, struct OPCIONES *op)
{
    struct stat st;
    struct passwd *pw;
    struct group *gr;
    struct tm *t;
    time_t fecha;
    char textoFecha[32], usuario[64], grupo[64], permisos[11], destino[PATH_MAX];
    ssize_t lon;

    if (lstat(ruta, &st) == -1) {
        ErrorSis("access", ruta);
        return;
    }
    // the date is also computed without -long because the reference shell
    // shows the access date in the short listing when -acc is given
    if (op->largo || op->acceso) {
        // stat does not store a creation date, so the modification date is used, like ls -l
        fecha = op->acceso ? st.st_atime : st.st_mtime;
        if ((t = localtime(&fecha)) != NULL)
            strftime(textoFecha, sizeof(textoFecha), "%Y/%m/%d-%H:%M", t);
        else
            snprintf(textoFecha, sizeof(textoFecha), "unknown date");
    }

    if (op->largo) {
        // if the user or group no longer exists they return NULL and the number is shown
        if ((pw = getpwuid(st.st_uid)) != NULL)
            snprintf(usuario, sizeof(usuario), "%s", pw->pw_name);
        else
            snprintf(usuario, sizeof(usuario), "%lu", (unsigned long) st.st_uid);
        if ((gr = getgrgid(st.st_gid)) != NULL)
            snprintf(grupo, sizeof(grupo), "%s", gr->gr_name);
        else
            snprintf(grupo, sizeof(grupo), "%lu", (unsigned long) st.st_gid);

        CadenaPermisos(st.st_mode, permisos);
        printf("%s %3lu (%8lu) %8s %8s %s %10lld %s",
               textoFecha, (unsigned long) st.st_nlink, (unsigned long) st.st_ino,
               usuario, grupo, permisos, (long long) st.st_size, nombre);
    } else if (op->acceso)
        printf("%10lld  %s %s", (long long) st.st_size, textoFecha, nombre);
    else
        printf("%10lld  %s", (long long) st.st_size, nombre);

    if (op->enlace && S_ISLNK(st.st_mode))
        if ((lon = readlink(ruta, destino, sizeof(destino) - 1)) != -1) {
            destino[lon] = '\0';   // readlink does not add the '\0' at the end
            printf(" -> %s", destino);
        }
    printf("\n");
}

// ListarContenido and RecorrerSubdirectorios call each other
void ListarContenido(const char *dir, struct OPCIONES *op);

void ListarDirectorio(const char *dir, struct OPCIONES *op)
{
    DIR *d;
    struct dirent *ent;
    char *ruta;

    if ((d = opendir(dir)) == NULL) {
        ErrorSis("open directory", dir);
        return;
    }
    printf("************%s\n", dir);
    while ((ent = readdir(d)) != NULL) {
        if (!op->ocultos && ent->d_name[0] == '.')
            continue;
        if ((ruta = UnirRuta(dir, ent->d_name)) == NULL) {
            printf("Cannot allocate memory\n");
            break;
        }
        ImprimirInfo(ruta, ent->d_name, op);
        free(ruta);
    }
    closedir(d);
}

void RecorrerSubdirectorios(const char *dir, struct OPCIONES *op)
{
    DIR *d;
    struct dirent *ent;
    struct stat st;
    char *ruta;

    if ((d = opendir(dir)) == NULL)
        return;   // the error is already shown by ListarDirectorio
    while ((ent = readdir(d)) != NULL) {
        // even with -hid, entering . or .. would make the recursion infinite
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        if (!op->ocultos && ent->d_name[0] == '.')
            continue;
        if ((ruta = UnirRuta(dir, ent->d_name)) == NULL) {
            printf("Cannot allocate memory\n");
            break;
        }
        // lstat and not d_type because the assignment requires it, and not stat so as not
        // to follow links to directories, which could form cycles
        if (lstat(ruta, &st) == 0 && S_ISDIR(st.st_mode))
            ListarContenido(ruta, op);
        free(ruta);
    }
    closedir(d);
}

void ListarContenido(const char *dir, struct OPCIONES *op)
{
    if (op->recursion == REC_ANTES)
        RecorrerSubdirectorios(dir, op);
    ListarDirectorio(dir, op);
    if (op->recursion == REC_DESPUES)
        RecorrerSubdirectorios(dir, op);
}

// esList because -hid, -reca and -recb are only valid for list
int LeerOpciones(char *arg[], int esList, struct OPCIONES *op)
{
    int i;

    memset(op, 0, sizeof(struct OPCIONES));
    for (i = 0; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "-long"))
            op->largo = 1;
        else if (!strcmp(arg[i], "-link"))
            op->enlace = 1;
        else if (!strcmp(arg[i], "-acc"))
            op->acceso = 1;
        else if (esList && !strcmp(arg[i], "-hid"))
            op->ocultos = 1;
        else if (esList && !strcmp(arg[i], "-reca"))
            op->recursion = REC_DESPUES;
        else if (esList && !strcmp(arg[i], "-recb"))
            op->recursion = REC_ANTES;
        else
            break;
    }
    return i;
}

void Cmd_listfile(char *arg[])
{
    struct OPCIONES op;
    int i = LeerOpciones(arg, 0, &op);

    if (arg[i] == NULL) {
        MostrarDirActual();
        return;
    }
    for (; arg[i] != NULL; i++)
        ImprimirInfo(arg[i], arg[i], &op);
}

void Cmd_list(char *arg[])
{
    struct OPCIONES op;
    struct stat st;
    int i = LeerOpciones(arg, 1, &op);

    if (arg[i] == NULL) {
        MostrarDirActual();
        return;
    }
    for (; arg[i] != NULL; i++) {
        if (lstat(arg[i], &st) == -1) {
            ErrorSis("access", arg[i]);
            continue;
        }
        if (S_ISDIR(st.st_mode))
            ListarContenido(arg[i], &op);
        else
            ImprimirInfo(arg[i], arg[i], &op);
    }
}

// the address comes in hex (with or without 0x); it is not checked to be valid
int LeerDireccion(char *s, void **p)
{
    char *fin;
    unsigned long v;

    errno = 0;
    v = strtoul(s, &fin, 16);
    if (errno != 0 || *s == '\0' || *fin != '\0' || s[0] == '-')
        return -1;
    *p = (void *) v;
    return 0;
}

// number of bytes in decimal, returns -1 if it is not valid
int LeerCantidad(char *s, size_t *cont)
{
    char *fin;
    unsigned long v;

    errno = 0;
    v = strtoul(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || s[0] == '-' || v > SSIZE_MAX)
        return -1;
    *cont = v;
    return 0;
}

void Cmd_read(char *arg[])
{
    int df;
    void *p;
    size_t cont;
    ssize_t leidos;

    if (NumArgs(arg) != 3) {
        printf("Usage: read df addr cont\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }
    if (LeerDireccion(arg[1], &p) == -1) {
        printf("Invalid address: %s\n", arg[1]);
        return;
    }
    if (LeerCantidad(arg[2], &cont) == -1) {
        printf("Invalid number of bytes: %s\n", arg[2]);
        return;
    }

    // with a bad address read fails with EFAULT, it does not crash the shell
    leidos = read(df, p, cont);
    if (leidos == -1)
        ErrorSis("read from descriptor", arg[0]);
    else
        printf("Read %ld bytes from descriptor %d into %p\n", (long) leidos, df, p);
}

void Cmd_write(char *arg[])
{
    int df;
    void *p;
    size_t cont;
    ssize_t escritos;

    if (NumArgs(arg) != 3) {
        printf("Usage: write df addr cont\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Invalid descriptor: %s\n", arg[0]);
        return;
    }
    if (LeerDireccion(arg[1], &p) == -1) {
        printf("Invalid address: %s\n", arg[1]);
        return;
    }
    if (LeerCantidad(arg[2], &cont) == -1) {
        printf("Invalid number of bytes: %s\n", arg[2]);
        return;
    }

    fflush(stdout);   // in case df is the standard output
    escritos = write(df, p, cont);
    if (escritos == -1)
        ErrorSis("write to descriptor", arg[0]);
    else
        printf("Wrote %ld bytes from %p to descriptor %d\n", (long) escritos, p, df);
}

#define TAMANO 2048

void Recursiva(int n)
{
    char automatico[TAMANO];
    static char estatico[TAMANO];

    printf("parameter:%3d(%p) array %p, static array %p\n",
           n, (void *) &n, (void *) automatico, (void *) estatico);
    if (n > 0)
        Recursiva(n - 1);
}

void Cmd_recurse(char *arg[])
{
    long n;
    char *fin;

    if (arg[0] == NULL) {
        printf("Usage: recurse n\n");
        return;
    }

    errno = 0;
    n = strtol(arg[0], &fin, 10);
    if (errno != 0 || *fin != '\0' || n < 0 || n > INT_MAX) {
        printf("Invalid number: %s\n", arg[0]);
        return;
    }
    Recursiva((int) n);
}

void LlenarMemoria(void *p, size_t cont, unsigned char byte)
{
    unsigned char *arr = p;
    size_t i;

    for (i = 0; i < cont; i++)
        arr[i] = byte;
}

void Cmd_memfill(char *arg[])
{
    void *p;
    size_t cont;
    unsigned long byte;
    char *fin;

    if (NumArgs(arg) != 3) {
        printf("Usage: memfill addr cont hcode\n");
        return;
    }
    if (LeerDireccion(arg[0], &p) == -1) {
        printf("Invalid address: %s\n", arg[0]);
        return;
    }
    if (LeerCantidad(arg[1], &cont) == -1) {
        printf("Invalid number of bytes: %s\n", arg[1]);
        return;
    }

    errno = 0;
    byte = strtoul(arg[2], &fin, 16);
    if (errno != 0 || *fin != '\0' || arg[2][0] == '-' || byte > 0xFF) {
        printf("Invalid byte: %s\n", arg[2]);
        return;
    }

    printf("Filling %lu bytes of memory with byte %c(%02lx) from address %p\n",
           (unsigned long) cont, isprint(byte) ? (int) byte : ' ', byte, p);
    fflush(stdout);   // so the message is shown even if the address is bad and it crashes
    LlenarMemoria(p, cont, (unsigned char) byte);
}

#define BYTES_LINEA 25

// each line shows the characters and below them their hex codes
void VolcarMemoria(void *p, size_t cont)
{
    unsigned char *arr = p;
    size_t i, j, lim;

    for (i = 0; i < cont; i += BYTES_LINEA) {
        lim = (i + BYTES_LINEA < cont) ? i + BYTES_LINEA : cont;
        for (j = i; j < lim; j++)
            printf("%2c ", isprint(arr[j]) ? arr[j] : ' ');
        printf("\n");
        for (j = i; j < lim; j++)
            printf("%02x ", arr[j]);
        printf("\n");
    }
}

void Cmd_memdump(char *arg[])
{
    void *p;
    size_t cont;

    if (NumArgs(arg) != 2) {
        printf("Usage: memdump addr cont\n");
        return;
    }
    if (LeerDireccion(arg[0], &p) == -1) {
        printf("Invalid address: %s\n", arg[0]);
        return;
    }
    if (LeerCantidad(arg[1], &cont) == -1) {
        printf("Invalid number of bytes: %s\n", arg[1]);
        return;
    }

    printf("Dumping %lu bytes from address %p\n", (unsigned long) cont, p);
    fflush(stdout);
    VolcarMemoria(p, cont);
}

/*************MEMORY (P2)************************/

bool isEmptyList(tList *list)
{
    return list->head == NULL;
}

// returns -1 if there is no memory for the node; the block itself is not freed here
int AddMemList(tList *list, void *p, size_t size, key_t key, memType type, int df, char *filename)
{
    node *pos = malloc(sizeof(node));

    if (pos == NULL)
        return -1;
    pos->filename = NULL;
    if (filename != NULL && (pos->filename = strdup(filename)) == NULL) {
        free(pos);
        return -1;
    }
    pos->address = p;
    pos->size = size;
    pos->key = key;
    pos->type = type;
    pos->df = df;
    pos->time = time(NULL);

    pos->next = NULL;
    pos->prev = list->tail;
    if (isEmptyList(list))
        list->head = pos;
    else
        list->tail->next = pos;
    list->tail = pos;
    return 0;
}

void RemoveList(tList *list, node *pos)
{
    if (pos->prev != NULL)
        pos->prev->next = pos->next;
    else
        list->head = pos->next;
    if (pos->next != NULL)
        pos->next->prev = pos->prev;
    else
        list->tail = pos->prev;
    free(pos->filename);
    free(pos);
}

// searches from the end, so if there are several matches the last one allocated is found
node *BuscarBloque(tList *list, memType type, size_t size, key_t key, char *filename)
{
    node *pos;

    for (pos = list->tail; pos != NULL; pos = pos->prev) {
        if (pos->type != type)
            continue;
        if (type == MALLOC_MEM && pos->size == size)
            return pos;
        if (type == SHARED_MEM && pos->key == key)
            return pos;
        if (type == MMAP_MEM && !strcmp(pos->filename, filename))
            return pos;
    }
    return NULL;
}

// frees the block (free, shmdt or munmap) and removes it from the list
void LiberarBloque(tList *list, node *pos)
{
    if (pos->type == MALLOC_MEM) {
        free(pos->address);
    }
    else if (pos->type == SHARED_MEM) {
        if (shmdt(pos->address) == -1) {
            printf("Cannot detach %p: %s\n", pos->address, strerror(errno));
            return;
        }
    }
    else {
        if (munmap(pos->address, pos->size) == -1) {
            ErrorSis("unmap", pos->filename);
            return;
        }
        // df is -1 if it was already closed with close -f
        if (pos->df != -1) {
            if (close(pos->df) == -1)
                ErrorSis("close the descriptor of", pos->filename);
            else
                EliminarFichero(pos->df);
        }
    }
    printf("Deallocated block at %p\n", pos->address);
    RemoveList(list, pos);
}

// tipo -1 shows all the blocks, otherwise only the ones of that type
void MemoryList(tList *list, int tipo)
{
    node *pos;
    char fecha[32];

    printf("******List of blocks allocated for process %d\n", (int) getpid());
    for (pos = list->head; pos != NULL; pos = pos->next) {
        if (tipo != -1 && (int) pos->type != tipo)
            continue;
        strftime(fecha, sizeof(fecha), "%b %d %H:%M", localtime(&pos->time));
        printf("%20p %12lu %s ", pos->address, (unsigned long) pos->size, fecha);
        if (pos->type == MALLOC_MEM)
            printf("malloc\n");
        else if (pos->type == SHARED_MEM)
            printf("shared (key %d)\n", (int) pos->key);
        else
            printf("%s (descriptor %d)\n", pos->filename, pos->df);
    }
}

// shared memory key in decimal, 0 (IPC_PRIVATE) is not allowed
int LeerClave(char *s, key_t *clave)
{
    char *fin;
    long v;

    errno = 0;
    v = strtol(s, &fin, 10);
    if (errno != 0 || *s == '\0' || *fin != '\0' || v <= 0 || v > INT_MAX)
        return -1;
    *clave = (key_t) v;
    return 0;
}

void AsignarMalloc(char *arg[])
{
    size_t tam;
    void *p;

    if (arg[0] == NULL) {
        MemoryList(&list, MALLOC_MEM);
        return;
    }
    if (LeerCantidad(arg[0], &tam) == -1 || tam == 0) {
        printf("Invalid size: %s\n", arg[0]);
        return;
    }
    if ((p = malloc(tam)) == NULL) {
        ErrorSis("allocate memory", NULL);
        return;
    }
    if (AddMemList(&list, p, tam, 0, MALLOC_MEM, -1, NULL) == -1) {
        printf("Cannot add the block to the list: not enough memory\n");
        free(p);
        return;
    }
    printf("Allocated %lu bytes at %p\n", (unsigned long) tam, p);
}

void AsignarCreateShared(char *arg[])
{
    key_t clave;
    size_t tam;
    int id;
    void *p;

    if (arg[0] == NULL) {
        MemoryList(&list, SHARED_MEM);
        return;
    }
    if (arg[1] == NULL) {
        printf("Usage: allocate -createshared cl siz\n");
        return;
    }
    if (LeerClave(arg[0], &clave) == -1) {
        printf("Invalid key: %s\n", arg[0]);
        return;
    }
    if (LeerCantidad(arg[1], &tam) == -1 || tam == 0) {
        printf("Invalid size: %s\n", arg[1]);
        return;
    }

    // IPC_EXCL makes it fail if the key is already in use
    if ((id = shmget(clave, tam, IPC_CREAT | IPC_EXCL | 0666)) == -1) {
        ErrorSis("create shared memory with key", arg[0]);
        return;
    }
    if ((p = shmat(id, NULL, 0)) == (void *) -1) {
        ErrorSis("attach shared memory with key", arg[0]);
        shmctl(id, IPC_RMID, NULL);   // we just created it, so we remove it
        return;
    }
    if (AddMemList(&list, p, tam, clave, SHARED_MEM, -1, NULL) == -1) {
        printf("Cannot add the block to the list: not enough memory\n");
        shmdt(p);
        return;
    }
    printf("Allocated shared memory (key %d) at %p\n", (int) clave, p);
}

void AsignarShared(char *arg[])
{
    key_t clave;
    int id;
    void *p;
    struct shmid_ds info;

    if (arg[0] == NULL) {
        MemoryList(&list, SHARED_MEM);
        return;
    }
    if (LeerClave(arg[0], &clave) == -1) {
        printf("Invalid key: %s\n", arg[0]);
        return;
    }

    if ((id = shmget(clave, 0, 0666)) == -1) {
        ErrorSis("get shared memory with key", arg[0]);
        return;
    }
    if (shmctl(id, IPC_STAT, &info) == -1) {
        ErrorSis("get information of shared memory with key", arg[0]);
        return;
    }
    if ((p = shmat(id, NULL, 0)) == (void *) -1) {
        ErrorSis("attach shared memory with key", arg[0]);
        return;
    }
    if (AddMemList(&list, p, info.shm_segsz, clave, SHARED_MEM, -1, NULL) == -1) {
        printf("Cannot add the block to the list: not enough memory\n");
        shmdt(p);
        return;
    }
    printf("Attached shared memory (key %d) at %p\n", (int) clave, p);
}

void AsignarMmap(char *arg[])
{
    int i, df, modo = O_RDONLY, prot = 0;
    struct stat st;
    void *p;

    if (arg[0] == NULL) {
        MemoryList(&list, MMAP_MEM);
        return;
    }
    if (arg[1] == NULL) {
        printf("Usage: allocate -mmap fil perm\n");
        return;
    }

    for (i = 0; arg[1][i] != '\0'; i++) {
        if (arg[1][i] == 'r')
            prot |= PROT_READ;
        else if (arg[1][i] == 'w') {
            prot |= PROT_WRITE;
            modo = O_RDWR;   // MAP_SHARED with write needs the file open for writing
        }
        else if (arg[1][i] == 'x')
            prot |= PROT_EXEC;
        else {
            printf("Invalid permissions: %s (use r, w and x)\n", arg[1]);
            return;
        }
    }

    if ((df = open(arg[0], modo)) == -1) {
        ErrorSis("open", arg[0]);
        return;
    }
    if (fstat(df, &st) == -1) {
        ErrorSis("access", arg[0]);
        close(df);
        return;
    }
    if (st.st_size == 0) {
        printf("Cannot map %s: the file is empty\n", arg[0]);
        close(df);
        return;
    }
    if ((p = mmap(NULL, st.st_size, prot, MAP_SHARED, df, 0)) == MAP_FAILED) {
        ErrorSis("map", arg[0]);
        close(df);
        return;
    }
    if (AddMemList(&list, p, st.st_size, -1, MMAP_MEM, df, arg[0]) == -1) {
        printf("Cannot add the block to the list: not enough memory\n");
        munmap(p, st.st_size);
        close(df);
        return;
    }
    // the descriptor stays open, so it also goes to the open files list
    if (AnadirFichero(df, modo, arg[0]) == -1)
        printf("Cannot add %s to the open files list: not enough memory\n", arg[0]);
    printf("File %s mapped at %p\n", arg[0], p);
}

void Cmd_allocate(char *arg[])
{
    if (arg[0] == NULL)
        MemoryList(&list, -1);
    else if (!strcmp(arg[0], "-malloc"))
        AsignarMalloc(arg + 1);
    else if (!strcmp(arg[0], "-createshared"))
        AsignarCreateShared(arg + 1);
    else if (!strcmp(arg[0], "-shared"))
        AsignarShared(arg + 1);
    else if (!strcmp(arg[0], "-mmap"))
        AsignarMmap(arg + 1);
    else
        printf("Usage: allocate [-malloc|-shared|-createshared|-mmap] ...\n");
}

void BorrarClave(char *s)
{
    key_t clave;
    int id;

    if (s == NULL) {
        printf("Usage: deallocate -delkey cl\n");
        return;
    }
    if (LeerClave(s, &clave) == -1) {
        printf("Invalid key: %s\n", s);
        return;
    }
    if ((id = shmget(clave, 0, 0666)) == -1) {
        ErrorSis("get shared memory with key", s);
        return;
    }
    if (shmctl(id, IPC_RMID, NULL) == -1) {
        ErrorSis("remove key", s);
        return;
    }
    printf("Key %s removed from the system\n", s);
}

void Cmd_deallocate(char *arg[])
{
    node *pos;
    size_t tam;
    key_t clave;
    void *p;

    if (arg[0] == NULL) {
        MemoryList(&list, -1);
        return;
    }

    if (!strcmp(arg[0], "-malloc")) {
        if (arg[1] == NULL) {
            MemoryList(&list, MALLOC_MEM);
            return;
        }
        if (LeerCantidad(arg[1], &tam) == -1 || tam == 0) {
            printf("Invalid size: %s\n", arg[1]);
            return;
        }
        if ((pos = BuscarBloque(&list, MALLOC_MEM, tam, 0, NULL)) == NULL) {
            printf("There is no malloc block of size %lu\n", (unsigned long) tam);
            return;
        }
    }
    else if (!strcmp(arg[0], "-shared")) {
        if (arg[1] == NULL) {
            MemoryList(&list, SHARED_MEM);
            return;
        }
        if (LeerClave(arg[1], &clave) == -1) {
            printf("Invalid key: %s\n", arg[1]);
            return;
        }
        if ((pos = BuscarBloque(&list, SHARED_MEM, 0, clave, NULL)) == NULL) {
            printf("There is no shared block with key %d\n", (int) clave);
            return;
        }
    }
    else if (!strcmp(arg[0], "-mmap")) {
        if (arg[1] == NULL) {
            MemoryList(&list, MMAP_MEM);
            return;
        }
        if ((pos = BuscarBloque(&list, MMAP_MEM, 0, 0, arg[1])) == NULL) {
            printf("File %s is not mapped\n", arg[1]);
            return;
        }
    }
    else if (!strcmp(arg[0], "-delkey")) {
        BorrarClave(arg[1]);
        return;
    }
    else if (arg[0][0] == '-') {
        printf("Usage: deallocate [-malloc|-shared|-delkey|-mmap|addr] ...\n");
        return;
    }
    else {
        if (LeerDireccion(arg[0], &p) == -1) {
            printf("Invalid address: %s\n", arg[0]);
            return;
        }
        for (pos = list.head; pos != NULL && pos->address != p; pos = pos->next)
            ;
        if (pos == NULL) {
            printf("Address %p was not allocated with allocate\n", p);
            return;
        }
    }

    LiberarBloque(&list, pos);
}

void Cmd_io(char *arg[])
{
    int n = NumArgs(arg), i = 1, df, flags;
    void *p;
    size_t cont;
    ssize_t res;

    if (n == 4 && !strcmp(arg[0], "read")) {
        if (LeerDireccion(arg[2], &p) == -1) {
            printf("Invalid address: %s\n", arg[2]);
            return;
        }
        if (LeerCantidad(arg[3], &cont) == -1) {
            printf("Invalid number of bytes: %s\n", arg[3]);
            return;
        }
        if ((df = open(arg[1], O_RDONLY)) == -1) {
            ErrorSis("open", arg[1]);
            return;
        }
        res = read(df, p, cont);
        if (res == -1)
            ErrorSis("read", arg[1]);
        else
            printf("Read %ld bytes from %s into %p\n", (long) res, arg[1], p);
        close(df);
        return;
    }

    if (n >= 1 && !strcmp(arg[0], "write")) {
        // without -o the file must not exist (O_EXCL), with -o it is overwritten
        flags = O_WRONLY | O_CREAT | O_EXCL;
        if (n == 5 && !strcmp(arg[1], "-o")) {
            flags = O_WRONLY | O_CREAT | O_TRUNC;
            i = 2;
        }
        if (n - i != 3) {
            printf("Usage: i-o write [-o] f addr cnt\n");
            return;
        }
        if (LeerDireccion(arg[i + 1], &p) == -1) {
            printf("Invalid address: %s\n", arg[i + 1]);
            return;
        }
        if (LeerCantidad(arg[i + 2], &cont) == -1) {
            printf("Invalid number of bytes: %s\n", arg[i + 2]);
            return;
        }
        if ((df = open(arg[i], flags, 0666)) == -1) {
            ErrorSis("open", arg[i]);
            return;
        }
        res = write(df, p, cont);
        if (res == -1)
            ErrorSis("write", arg[i]);
        else
            printf("Wrote %ld bytes from %p into %s\n", (long) res, p, arg[i]);
        close(df);
        return;
    }

    printf("Usage: i-o [read|write] [-o] f addr cnt\n");
}

// external variables for memory -vars
int ext1 = 1, ext2 = 2, ext3 = 3;
int extN1, extN2, extN3;

void Blocks()
{
    MemoryList(&list, -1);
}

void Funcs()
{
    printf("Shell functions:   %p, %p, %p\n",
           (void *) Cmd_allocate, (void *) Cmd_deallocate, (void *) MemoryList);
    printf("Library functions: %p, %p, %p\n",
           (void *) malloc, (void *) printf, (void *) strcmp);
}

void Vars()
{
    int a1 = 1, a2 = 2, a3 = 3;
    int b1, b2, b3;
    static int s1 = 1, s2 = 2, s3 = 3;
    static int t1, t2, t3;

    printf("Automatic variables:         %p, %p, %p\n", (void *) &a1, (void *) &a2, (void *) &a3);
    printf("Automatic (not initialized): %p, %p, %p\n", (void *) &b1, (void *) &b2, (void *) &b3);
    printf("Static variables:            %p, %p, %p\n", (void *) &s1, (void *) &s2, (void *) &s3);
    printf("Static (not initialized):    %p, %p, %p\n", (void *) &t1, (void *) &t2, (void *) &t3);
    printf("External variables:          %p, %p, %p\n", (void *) &ext1, (void *) &ext2, (void *) &ext3);
    printf("External (not initialized):  %p, %p, %p\n", (void *) &extN1, (void *) &extN2, (void *) &extN3);
}

void Pmap()
{
    char pid[32];
    pid_t hijo;

    sprintf(pid, "%d", (int) getpid());
    fflush(stdout);   // so the child does not get a copy of what is still in the buffer
    if ((hijo = fork()) == -1) {
        ErrorSis("create process", NULL);
        return;
    }
    if (hijo == 0) {
        execlp("pmap", "pmap", pid, NULL);
        ErrorSis("execute", "pmap");
        _exit(1);
    }
    waitpid(hijo, NULL, 0);
}

void Cmd_memory(char *arg[])
{
    int i;

    if (arg[0] == NULL) {
        Blocks();
        Funcs();
        Vars();
        return;
    }
    for (i = 0; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "-blocks"))
            Blocks();
        else if (!strcmp(arg[i], "-funcs"))
            Funcs();
        else if (!strcmp(arg[i], "-vars"))
            Vars();
        else if (!strcmp(arg[i], "-all")) {
            Blocks();
            Funcs();
            Vars();
        }
        else if (!strcmp(arg[i], "-pmap"))
            Pmap();
        else
            printf("Usage: memory [-blocks|-funcs|-vars|-all|-pmap]\n");
    }
}

/**************************SHELL**************************/

struct COMANDO {
    char *nombre;
    void (*funcion)(char **);
};

static struct COMANDO C[] = {
    {"authors", Cmd_authors},
    {"date", Cmd_date},
    {"exit", Cmd_fin},
    {"bye", Cmd_fin},
    {"pid", Cmd_pid},
    {"sysinfo", Cmd_sysinfo},
    {"help", Cmd_help},
    {"chdir", Cmd_chdir},
    {"open", Cmd_open},
    {"close", Cmd_close},
    {"listopen", Cmd_listopen},
    {"dup", Cmd_dup},
    {"lseek", Cmd_lseek},
    {"readstr", Cmd_readstr},
    {"writestr", Cmd_writestr},
    {"makefile", Cmd_makefile},
    {"makedir", Cmd_makedir},
    {"delete", Cmd_delete},
    {"deltree", Cmd_deltree},
    {"listfile", Cmd_listfile},
    {"list", Cmd_list},
    {"read", Cmd_read},
    {"write", Cmd_write},
    {"recurse", Cmd_recurse},
    {"memfill", Cmd_memfill},
    {"memdump", Cmd_memdump},
    {"allocate", Cmd_allocate},
    {"deallocate", Cmd_deallocate},
    {"i-o", Cmd_io},
    {"memory", Cmd_memory},
    {NULL, NULL}            /*NULL marks the end of the array*/
};

void DecidirComando(char *tr[])
{
    int i;

    if (tr[0] == NULL) return;
    for (i = 0; C[i].nombre != NULL; i++)
        if (!strcmp(C[i].nombre, tr[0])) {
            (*C[i].funcion)(tr + 1);
            return;
        }
    printf("%s: command not found\n", tr[0]);
}

int TrocearCadena(char *cadena, char *trozos[])
{
    int i = 1;

    if ((trozos[0] = strtok(cadena, " \n\t")) == NULL)
        return 0;
    while ((trozos[i] = strtok(NULL, " \n\t")) != NULL)
        i++;
    return i;
}

void ProcesarEntrada(char *entrada)
{
    char *tr[MAXENTRADA / 2];

    if (TrocearCadena(entrada, tr) == 0)    /*nothing to do*/
        return;
    DecidirComando(tr);
}

int main()
{
    char entrada[MAXENTRADA];

    CargarHeredados();

    while (1) {
        printf("-> ");
        fflush(stdout);
        if (fgets(entrada, MAXENTRADA, stdin) == NULL) {   // ctrl-D
            printf("\n");
            Cmd_fin(NULL);
        }
        ProcesarEntrada(entrada);
        fflush(stdout);
    }
}