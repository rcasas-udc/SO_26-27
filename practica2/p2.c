/*
 * Operating Systems - Lab Assignment 1
 *
 * Authors:
 *   Francisco Martínez Rubido    login: f.martinezr@udc.es
 *   Román Casas Riveira          login: r.casas@udc.es
 */

#define _GNU_SOURCE
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
    int i, df;
    char *num = NULL;

    for (i = 0; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "-f"))
            continue;
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

    fflush(stdout);   // in case we close the standard output
    if (close(df) == -1) {
        ErrorSis("close descriptor", num);
        return;
    }
    EliminarFichero(df);
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

bool isEmptyList(tList *list){
    if (list->head==NULL){
        return true;
    }
    else
        return false;
}

void AddMemList(tList *list, void *p, size_t size, key_t key, memType type, int df, char *filename){
    node *pos= malloc(sizeof(node));
    if (pos != NULL){
        pos->address=p;
        pos->type=type;
        pos->size=size;
        pos->key=key;
        pos->df=df;
        if (filename != NULL)
            pos->filename=strdup(filename);
        else
            pos->filename=NULL;

        pos->next=NULL;
        if (isEmptyList(list)){
            pos->prev=NULL;
            list->head=pos;
            list->tail=pos;
        }
        else{
            pos->prev=list->tail;
            list->tail->next=pos;
            list->tail=pos;
        }
    }
    else
        ErrorSis("Impossible to add to memory list", NULL);
}

void RemoveList(tList *list, node *pos){
            free(pos->filename);
            if (pos->prev!=NULL){
                pos->prev->next=pos->next;
            }
            else
                list->head=pos->next;
            if (pos->next!=NULL){
                pos->next->prev=pos->prev;
            }
            else
                list->tail=pos->prev;
            free(pos);
}

void DelMemList (tList *list, memType type, size_t size, key_t key, char *filename){
    if (type==MALLOC_MEM){
        node *pos=list->tail;
        while (pos!=NULL && !(pos->size==size && pos->type==type)){
            pos=pos->prev;
        }
        if (pos!=NULL){
            free(pos->address);
            RemoveList(list, pos);
        }
        else
            ErrorSis("Block not found", NULL);
    }
    else if (type==SHARED_MEM){
        node *pos=list->tail;
        while (pos!=NULL && !(pos->key==key && pos->type==type)){
            pos=pos->prev;
        }
        if (pos!=NULL){
            if (shmdt(pos->address)==-1){
                ErrorSis("Impossible to detach from memory", NULL);
            }
            RemoveList(list, pos);
        }
        else
            ErrorSis("Block not found", NULL);
    }
    else if (type==MMAP_MEM){
        node *pos=list->tail;
        if (filename!=NULL){
            while (pos!=NULL && !(pos->type==type && strcmp(pos->filename, filename)==0)){
                pos=pos->prev;
            }
            if (pos!=NULL){
                if (munmap(pos->address, pos->size)){
                    ErrorSis("Impossible to unmap file", NULL);
                }
                if(close(pos->df)){
                    ErrorSis("Impossible to close descriptor file", NULL);
                }
                RemoveList(list, pos);
            }
            else
                ErrorSis("Block not found", NULL);
        }
        else
            ErrorSis("Invalid filename", NULL);
    }
    else
        ErrorSis("Not a valid memory type", NULL);
}

void MemoryList(tList *list){
    node *pos=list->head;
    while (pos!=NULL){
        if (pos->filename != NULL){
            printf("%p, %d, %zu, %d, %s\n",pos->address, pos->type, pos->size, pos->key, pos->filename);
        }
        else
            printf("%p, %d, %zu, %d\n",pos->address, pos->type, pos->size, pos->key);
        pos=pos->next;
    }
}

void Cmd_allocate(char *arg[]){
    if (arg[0] != NULL){
        if (strcmp(arg[0], "-malloc") == 0){
            if (arg[1] != NULL){
                size_t size = (size_t) strtoul(arg[1], NULL, 10);
                void *p = malloc(size);
                if (p==NULL){
                    ErrorSis("Impossible to allocate", NULL);
                }
                else
                    AddMemList(&list, p, size, 0, MALLOC_MEM, -1, NULL);
            }
        }
        else if (strcmp(arg[0], "-createshared") == 0){
            if (arg[1] != NULL && arg[2] != NULL){
                key_t key = (key_t) strtoul(arg[1], NULL, 0);
                size_t size = (size_t) strtoul(arg[2], NULL, 10);
                int shmid = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
                if (shmid==-1){
                    ErrorSis("Impossible to allocate", NULL);
                }
                else{
                    void *p = shmat(shmid, NULL, 0);
                    if (p == (void *)-1){
                        ErrorSis("Impossible to allocate", NULL);
                    }
                    else
                        AddMemList(&list, p, size, key, SHARED_MEM, -1, NULL);
                }
            }
        }
        else if (strcmp(arg[0], "-shared")==0){
            if (arg[1] != NULL){
                key_t key = (key_t) strtoul(arg[1], NULL, 0);
                int shmid = shmget(key, 0, 0666);
                if (shmid==-1){
                ErrorSis("Impossible to allocate", NULL);
                }
                else{
                    void *p = shmat(shmid, NULL, 0);
                    struct shmid_ds info;
                    if (p == (void *)-1){
                        ErrorSis("Impossible to allocate", NULL);
                    }
                    else if (shmctl(shmid, IPC_STAT, &info)==-1){
                        ErrorSis("Impossible to get allocated memory information", NULL);
                    }
                    else
                        AddMemList(&list, p, info.shm_segsz, key, SHARED_MEM, -1, NULL);
                }
            }
        }
        else if (strcmp(arg[0], "-mmap")==0){
            if (arg[1]!=NULL && arg[2]!=NULL){
                struct stat st;
                    int perm=O_RDONLY;
                    for (int i=0; arg[2][i] != '\0'; i++){
                        if (arg[2][i] == 'w')
                            perm|=O_RDWR;
                    }
                    int prot=0;
                    for (int i=0; arg[2][i] != '\0'; i++){
                        if (arg[2][i] == 'r')
                            prot|=PROT_READ;
                        if (arg[2][i] == 'w')
                            prot|=PROT_WRITE;
                    }
                int df = open(arg[1], perm);
                if (df==-1)
                    ErrorSis("Impossible to get descriptor file", NULL);
                else{
                    if (fstat(df, &st) == 0){
                        size_t size = st.st_size;
                            if (prot==0){
                                close(df);
                                ErrorSis("Impossible to get permissions", NULL);
                            }
                            else{
                                void *p = mmap(NULL, size, prot, MAP_SHARED, df, 0);
                                if (p == (void *) -1){
                                    close(df);
                                    ErrorSis("Impossible to allocate", NULL);
                                }

                                else
                                    AddMemList(&list, p, size, -1, MMAP_MEM, df, arg[1]);
                            }
                        }
                    else{
                        close(df);
                        ErrorSis("Impossible to get information from descriptor file", NULL);
                    }
                }
            }
        }
    }
    else
        MemoryList(&list);
}

void Cmd_deallocate(char *arg[]){
    if (arg[0] != NULL){
        if (strcmp(arg[0], "-malloc") == 0){
            if (arg[1]!=NULL){
                size_t size = (size_t)strtoul(arg[1], NULL, 10);
                if (size==0){
                    ErrorSis("Impossible to get size", NULL);
                }
                else
                    DelMemList (&list, MALLOC_MEM, size, 0, NULL);
            }
            else
                ErrorSis("Invalid second argument", NULL);
        }
        else if (strcmp(arg[0], "-shared") == 0){
            if (arg[1]!=NULL){
                key_t key = (key_t) strtoul(arg[1], NULL, 0);
                DelMemList (&list, SHARED_MEM, 0, key, NULL);
            }
            else
                ErrorSis("Invalid second argument", NULL);
        }
        else if (strcmp(arg[0], "-mmap") == 0){
            if (arg[1]!=NULL)
                DelMemList (&list, MMAP_MEM, 0, 0, arg[1]);
            else
                ErrorSis("Invalid second argument", NULL);
        }
        else if (strcmp(arg[0], "-delkey") == 0){
            if (arg[1]!=NULL){
                key_t key = (key_t) strtoul(arg[1], NULL, 0);
                int shmid = shmget(key, 0, 0666);
                if (shmid==-1)
                    ErrorSis("Impossible to get shared memory block", NULL);
                else{
                    int delk = shmctl(shmid, IPC_RMID, NULL);
                    if (delk==-1)
                        ErrorSis("Impossible to delete key", NULL);
                }
            }
            else
                ErrorSis("Invalid argument", NULL);
        }
        else if (arg[0][0]=='-')
            ErrorSis("Unrecognized command", NULL);
        else {
            void *ad = (void *)strtoul(arg[0], NULL, 16);
            node *pos=list.head;
            while (pos!=NULL && pos->address!=ad){
                pos=pos->next;
            }
            if (pos!=NULL){
                if (pos->type==MALLOC_MEM){
                    free(pos->address);
                    RemoveList(&list, pos);
                }
                else if(pos->type==SHARED_MEM){
                    if (shmdt(pos->address)==-1){
                        ErrorSis("Impossible to detach from memory", NULL);
                    }
                    RemoveList(&list, pos);
                }
                else if(pos->type==MMAP_MEM){
                    if (munmap(pos->address, pos->size)){
                        ErrorSis("Impossible to unmap file", NULL);
                    }
                    if(close(pos->df)){
                        ErrorSis("Impossible to close descriptor file", NULL);
                    }
                    RemoveList(&list, pos);
                }
            }
            else
                ErrorSis("Address is not in the memory list", NULL);
        }
    }
    else
        MemoryList(&list);
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
