/*
 * Sistemas Operativos - Practica 1
 *
 * Autores:
 *   Francisco Martínez Rubido    login: f.martinezr@udc.es
 *   Román Casas Riveira          login: r.casas@udc.es
 */

#define _GNU_SOURCE
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

#define MAXENTRADA 2048

struct fichero {
    int df;
    int modo;
    char *nombre;
    struct fichero *sig;
};

struct fichero *lista = NULL;


// mete el fichero en la lista ordenado por df, si ya estaba lo actualiza
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

// saca el nombre real de /proc, si no se puede pone uno generico
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
        strcpy(nombre, "entrada estandar");
    else if (df == 1)
        strcpy(nombre, "salida estandar");
    else if (df == 2)
        strcpy(nombre, "error estandar");
    else
        snprintf(nombre, tam, "descriptor heredado %d", df);
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
            continue;   // no esta abierto
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
        // el modo actual se pide a fcntl para que salga igual que en lsof,
        // pero O_CREAT, O_EXCL y O_TRUNC solo los sabemos de cuando se abrio
        modo = fcntl(p->df, F_GETFL);
        if (modo == -1)
            modo = p->modo;
        modo |= p->modo & (O_CREAT | O_EXCL | O_TRUNC);

        ModoATexto(modo, texto);
        printf("descriptor: %2d -> %-30s %s\n", p->df, p->nombre, texto);
    }
}

// para los errores de llamadas al sistema, objeto puede ser NULL
void ErrorSis(const char *accion, const char *objeto)
{
    int err = errno;

    if (objeto != NULL)
        printf("Imposible %s %s: %s\n", accion, objeto, strerror(err));
    else
        printf("Imposible %s: %s\n", accion, strerror(err));
}

// pasa la cadena a descriptor, devuelve -1 si no es un numero valido
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


/*************COMANDOS DEL SHELL************************/
/* todos reciben los trozos sin el nombre del comando: arg[0] es el primer argumento */

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
            printf("Opcion no valida: %s\n", arg[0]);
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
            printf("Opcion no valida: %s\n", arg[0]);
            return;
        }
    }
    if (t == NULL) {
        printf("Imposible obtener la fecha\n");
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
            printf("Modo no valido: %s\n", arg[i]);
            return;
        }
    }

    df = open(arg[0], modo, 0777);
    if (df == -1) {
        ErrorSis("abrir", arg[0]);
        return;
    }
    if (AnadirFichero(df, modo, arg[0]) == -1) {
        printf("Imposible anadir %s a la lista: memoria insuficiente\n", arg[0]);
        close(df);
        return;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", df);
}

void Cmd_close(char *arg[])
{
    int i, df;
    char *num = NULL;

    // vale "close df", "close df -f" y "close -f df"
    // -f de momento no hace nada distinto porque aun no hay mapeos
    for (i = 0; arg[i] != NULL; i++) {
        if (!strcmp(arg[i], "-f"))
            continue;
        if (num != NULL || LeerDf(arg[i], &df) == -1) {
            printf("Uso: close df [-f]\n");
            return;
        }
        num = arg[i];
    }
    if (num == NULL) {
        ListarAbiertos();
        return;
    }

    fflush(stdout);   // por si cerramos la salida estandar
    if (close(df) == -1) {
        ErrorSis("cerrar el descriptor", num);
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
        printf("Descriptor no valido: %s\n", arg[0]);
        return;
    }

    fflush(stdout);
    if ((nuevo = dup(df)) == -1) {
        ErrorSis("duplicar el descriptor", arg[0]);
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
        printf("Imposible anadir el duplicado a la lista: memoria insuficiente\n");
        close(nuevo);
        return;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", nuevo);
}

void Cmd_lseek(char *arg[])
{
    int df, ref;
    off_t pos, res;
    char *fin;

    if (NumArgs(arg) != 3) {
        printf("Uso: lseek df pos SEEK_SET|SEEK_CUR|SEEK_END\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Descriptor no valido: %s\n", arg[0]);
        return;
    }

    errno = 0;
    pos = strtoll(arg[1], &fin, 10);
    if (errno != 0 || *fin != '\0') {
        printf("Posicion no valida: %s\n", arg[1]);
        return;
    }

    if (!strcmp(arg[2], "SEEK_SET"))
        ref = SEEK_SET;
    else if (!strcmp(arg[2], "SEEK_CUR"))
        ref = SEEK_CUR;
    else if (!strcmp(arg[2], "SEEK_END"))
        ref = SEEK_END;
    else {
        printf("Referencia no valida: %s (usar SEEK_SET, SEEK_CUR o SEEK_END)\n", arg[2]);
        return;
    }

    res = lseek(df, pos, ref);
    if (res == -1)
        ErrorSis("posicionar el descriptor", arg[0]);
    else
        printf("Nuevo offset: %lld\n", (long long) res);
}

void Cmd_readstr(char *arg[])
{
    int df;
    long cont;
    ssize_t leidos;
    char *fin, *buf;

    if (NumArgs(arg) != 2) {
        printf("Uso: readstr df cont\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Descriptor no valido: %s\n", arg[0]);
        return;
    }

    errno = 0;
    cont = strtol(arg[1], &fin, 10);
    if (errno != 0 || *fin != '\0' || cont < 0 || cont >= SSIZE_MAX) {
        printf("Numero de bytes no valido: %s\n", arg[1]);
        return;
    }

    buf = malloc(cont + 1);   // +1 para el '\0'
    if (buf == NULL) {
        printf("Imposible reservar %ld bytes\n", cont);
        return;
    }

    leidos = read(df, buf, cont);
    if (leidos == -1) {
        ErrorSis("leer del descriptor", arg[0]);
        free(buf);
        return;
    }
    buf[leidos] = '\0';
    printf("%s\n", buf);
    printf("Leidos %ld bytes del descriptor %d\n", (long) leidos, df);
    free(buf);
}

void Cmd_writestr(char *arg[])
{
    int df, i, tam = 0;
    ssize_t escritos;
    char *cad;

    if (NumArgs(arg) < 2) {
        printf("Uso: writestr df str\n");
        return;
    }
    if (LeerDf(arg[0], &df) == -1) {
        printf("Descriptor no valido: %s\n", arg[0]);
        return;
    }

    // strtok separa por espacios, asi que hay que volver a juntar los trozos
    for (i = 1; arg[i] != NULL; i++)
        tam += strlen(arg[i]) + 1;
    cad = malloc(tam);
    if (cad == NULL) {
        printf("Imposible reservar memoria\n");
        return;
    }
    strcpy(cad, arg[1]);
    for (i = 2; arg[i] != NULL; i++) {
        strcat(cad, " ");
        strcat(cad, arg[i]);
    }

    escritos = write(df, cad, strlen(cad));
    if (escritos == -1)
        ErrorSis("escribir en el descriptor", arg[0]);
    else
        printf("Escritos %ld bytes en el descriptor %d\n", (long) escritos, df);
    free(cad);
}

// la ruta se reserva con malloc para no tener limite de longitud en la recursion
char *UnirRuta(const char *dir, const char *nombre)
{
    size_t lon = strlen(dir) + strlen(nombre) + 2;
    char *ruta = malloc(lon);

    if (ruta != NULL)
        snprintf(ruta, lon, "%s/%s", dir, nombre);
    return ruta;
}

void MostrarDirActual()
{
    char dir[PATH_MAX];

    if (getcwd(dir, sizeof(dir)) == NULL)
        ErrorSis("obtener el directorio actual", NULL);
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

// se reciben ruta y nombre por separado porque al listar un directorio
// lstat necesita la ruta completa pero solo se muestra el nombre
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
        ErrorSis("acceder a", ruta);
        return;
    }
    // la fecha se calcula tambien sin -long porque la shell de referencia
    // muestra la de acceso en el listado corto cuando hay -acc
    if (op->largo || op->acceso) {
        // stat no guarda la fecha de creacion, se usa la de modificacion como ls -l
        fecha = op->acceso ? st.st_atime : st.st_mtime;
        if ((t = localtime(&fecha)) != NULL)
            strftime(textoFecha, sizeof(textoFecha), "%Y/%m/%d-%H:%M", t);
        else
            snprintf(textoFecha, sizeof(textoFecha), "fecha desconocida");
    }

    if (op->largo) {
        // si el usuario o el grupo ya no existen devuelven NULL y se muestra el numero
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
            destino[lon] = '\0';   // readlink no pone el '\0' al final
            printf(" -> %s", destino);
        }
    printf("\n");
}

// ListarContenido y RecorrerSubdirectorios se llaman entre si
void ListarContenido(const char *dir, struct OPCIONES *op);

void ListarDirectorio(const char *dir, struct OPCIONES *op)
{
    DIR *d;
    struct dirent *ent;
    char *ruta;

    if ((d = opendir(dir)) == NULL) {
        ErrorSis("abrir el directorio", dir);
        return;
    }
    printf("************%s\n", dir);
    while ((ent = readdir(d)) != NULL) {
        if (!op->ocultos && ent->d_name[0] == '.')
            continue;
        if ((ruta = UnirRuta(dir, ent->d_name)) == NULL) {
            printf("Imposible reservar memoria\n");
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
        return;   // el error ya lo muestra ListarDirectorio
    while ((ent = readdir(d)) != NULL) {
        // aunque haya -hid, entrar en . o .. haria la recursion infinita
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, ".."))
            continue;
        if (!op->ocultos && ent->d_name[0] == '.')
            continue;
        if ((ruta = UnirRuta(dir, ent->d_name)) == NULL) {
            printf("Imposible reservar memoria\n");
            break;
        }
        // lstat y no d_type porque lo pide el enunciado, y no stat para no
        // seguir enlaces a directorios, que podrian formar ciclos
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

// esList porque -hid, -reca y -recb solo valen para list
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
            ErrorSis("acceder a", arg[i]);
            continue;
        }
        if (S_ISDIR(st.st_mode))
            ListarContenido(arg[i], &op);
        else
            ImprimirInfo(arg[i], arg[i], &op);
    }
}

// TODO: ir quitando estos segun los hagamos
void Cmd_pendiente(char *arg[])
{
    printf("Comando no implementado todavia\n");
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
    {"pid", Cmd_pendiente},
    {"sysinfo", Cmd_pendiente},
    {"help", Cmd_pendiente},
    {"chdir", Cmd_pendiente},
    {"open", Cmd_open},
    {"close", Cmd_close},
    {"listopen", Cmd_pendiente},
    {"dup", Cmd_dup},
    {"lseek", Cmd_lseek},
    {"readstr", Cmd_readstr},
    {"writestr", Cmd_writestr},
    {"makefile", Cmd_pendiente},
    {"makedir", Cmd_pendiente},
    {"delete", Cmd_pendiente},
    {"deltree", Cmd_pendiente},
    {"listfile", Cmd_listfile},
    {"list", Cmd_list},
    {NULL, NULL}            /*NULL marca el final del array*/
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
    printf("%s: comando no encontrado\n", tr[0]);
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

    if (TrocearCadena(entrada, tr) == 0)    /*no hay nada*/
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