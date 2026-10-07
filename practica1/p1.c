/*
 * Operating Systems
 * Lab Assignment 1
 * 
 * Authors:
 *   Francisco Martínez Rubido    login: f.martinezr@udc.es
 *   Román Casas Riveira    login: r.casas@udc.es
 *
 * Compilación: gcc -Wall p1.c -o p1
 *
 * Solo están implementados authors, date, exit y bye. El resto de
 * comandos están registrados en la tabla COMANDOS pero apuntan a
 * cmdNoImplementado. Para implementar uno:
 *   1. Escribir su función con la firma  int cmdNombre(int n, char *tr[]), cmdNombre siendo el nombre del comando
 *   2. Cambiar cmdNoImplementado por cmdNombre en su línea de la tabla
 */

#define _DEFAULT_SOURCE
#define _XOPEN_SOURCE 700
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>    
#include <limits.h>    
#include <errno.h>     

#define MAXENTRADA 1024
#define MAXTROZOS  512
#define PROMPT     "-> "


typedef struct NodoFichero {
    int df;
    int modo;
    char *nombre;
    struct NodoFichero *sig;
} NodoFichero;

static NodoFichero *ficheros = NULL;

/* Inserta (ordenado por df) o actualiza una entrada. Devuelve -1 si no hay memoria */
static int insertarFichero(int df, int modo, const char *nombre)
{
    NodoFichero **p, *nuevo;
    char *copia;

    if ((copia = strdup(nombre)) == NULL)
        return -1;

    for (p = &ficheros; *p != NULL && (*p)->df < df; p = &(*p)->sig)
        ;

    if (*p != NULL && (*p)->df == df) {     /* ya existía: se actualiza */
        free((*p)->nombre);
        (*p)->nombre = copia;
        (*p)->modo = modo;
        return 0;
    }

    if ((nuevo = malloc(sizeof(NodoFichero))) == NULL) {
        free(copia);
        return -1;
    }
    nuevo->df = df;
    nuevo->modo = modo;
    nuevo->nombre = copia;
    nuevo->sig = *p;
    *p = nuevo;
    return 0;
}

static NodoFichero *buscarFichero(int df)
{
    NodoFichero *p;

    for (p = ficheros; p != NULL; p = p->sig)
        if (p->df == df)
            return p;
    return NULL;
}

static void eliminarFichero(int df)
{
    NodoFichero **p, *borrar;

    for (p = &ficheros; *p != NULL; p = &(*p)->sig) {
        if ((*p)->df == df) {
            borrar = *p;
            *p = borrar->sig;
            free(borrar->nombre);
            free(borrar);
            return;
        }
    }
}

static void borrarListaFicheros(void)
{
    NodoFichero *p;

    while (ficheros != NULL) {
        p = ficheros;
        ficheros = p->sig;
        free(p->nombre);
        free(p);
    }
}

/* Convierte unos flags de open/fcntl en texto: "O_RDWR O_APPEND ..." */
static void modoACadena(int modo, char *buf, size_t tam)
{
    size_t usado;

    switch (modo & O_ACCMODE) {
        case O_RDONLY: snprintf(buf, tam, "O_RDONLY"); break;
        case O_WRONLY: snprintf(buf, tam, "O_WRONLY"); break;
        case O_RDWR:   snprintf(buf, tam, "O_RDWR");   break;
        default:       snprintf(buf, tam, "O_???");    break;
    }

#define ANADIR_FLAG(F)                                          \
    if (modo & F) {                                             \
        usado = strlen(buf);                                    \
        snprintf(buf + usado, tam - usado, " %s", #F);          \
    }
    ANADIR_FLAG(O_CREAT)
    ANADIR_FLAG(O_EXCL)
    ANADIR_FLAG(O_TRUNC)
    ANADIR_FLAG(O_APPEND)
    ANADIR_FLAG(O_NONBLOCK)
#undef ANADIR_FLAG
}

/* Nombre de un descriptor heredado. En Linux se lee de /proc/self/fd */
static void nombreDescriptor(int df, char *nombre, size_t tam)
{
    char enlace[64];
    ssize_t n;

    snprintf(enlace, sizeof(enlace), "/proc/self/fd/%d", df);
    n = readlink(enlace, nombre, tam - 1);
    if (n != -1) {
        nombre[n] = '\0';
        return;
    }
    switch (df) {
        case 0:  snprintf(nombre, tam, "entrada estandar"); break;
        case 1:  snprintf(nombre, tam, "salida estandar");  break;
        case 2:  snprintf(nombre, tam, "error estandar");   break;
        default: snprintf(nombre, tam, "descriptor heredado %d", df); break;
    }
}

/* Mete en la lista todos los descriptores que el shell hereda de su padre */
static void cargarDescriptoresHeredados(void)
{
    long max = sysconf(_SC_OPEN_MAX);
    char nombre[PATH_MAX];
    int df, modo;

    if (max < 0 || max > 65536)
        max = 65536;

    for (df = 0; df < max; df++) {
        if ((modo = fcntl(df, F_GETFL)) == -1)
            continue;                   /* df no está abierto */
        nombreDescriptor(df, nombre, sizeof(nombre));
        insertarFichero(df, modo, nombre);
    }
}

static void listarFicherosAbiertos(void)
{
    NodoFichero *p;
    char textoModo[128];
    int modoActual, modo;

    for (p = ficheros; p != NULL; p = p->sig) {
        /* Los flags de acceso y estado se consultan al sistema para que
         * coincidan con lsof; O_CREAT, O_EXCL y O_TRUNC solo se conocen
         * al abrir, así que se toman de los guardados en la lista */
        modoActual = fcntl(p->df, F_GETFL);
        modo = (modoActual != -1) ? modoActual : p->modo;
        modo |= p->modo & (O_CREAT | O_EXCL | O_TRUNC);
        modoACadena(modo, textoModo, sizeof(textoModo));
        printf("descriptor: %2d -> %-30s %s\n", p->df, p->nombre, textoModo);
    }
}

/* Divide la línea en palabras. Devuelve el número de trozos.
 * trozos[0] es el nombre del comando y trozos[n] queda a NULL. */
static int trocearCadena(char *cadena, char *trozos[])
{
    int i;

    if ((trozos[0] = strtok(cadena, " \t\n")) == NULL)
        return 0;
    i = 1;
    while (i < MAXTROZOS - 1 && (trozos[i] = strtok(NULL, " \t\n")) != NULL)
        i++;
    trozos[i] = NULL;
    return i;
}

/* Informa de un error de una llamada al sistema (usa errno) */
static void errorSistema(const char *accion, const char *objeto)
{
    int err = errno;

    if (objeto != NULL)
        printf("Imposible %s %s: %s\n", accion, objeto, strerror(err));
    else
        printf("Imposible %s: %s\n", accion, strerror(err));
}

/* Convierte una cadena en descriptor. Devuelve -1 si no es válida */
static int leerDescriptor(const char *s, int *df)
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

/* ======================================================================
 *  COMANDOS
 *  Cada comando recibe el número de trozos (n) y los trozos (tr), donde
 *  tr[0] es el nombre del comando y tr[1]..tr[n-1] sus argumentos.
 *  Devuelven 1 si el shell debe terminar y 0 en otro caso.
 * ====================================================================== */

static int cmdAutores(int n, char *tr[])
{
    const char *nombres[] = {"Francisco Martínez Rubido", "Román Casas Riveira"};
    const char *logins[]  = {"f.martinezr@udc.es", "r.casas@udc.es"};
    int verNombres = 1, verLogins = 1, i;

    if (n > 1) {
        if (strcmp(tr[1], "-l") == 0)
            verNombres = 0;
        else if (strcmp(tr[1], "-n") == 0)
            verLogins = 0;
        else {
            printf("Opcion no valida: %s\n", tr[1]);
            return 0;
        }
    }

    for (i = 0; i < 2; i++) {
        if (verNombres && verLogins)
            printf("%s: %s\n", nombres[i], logins[i]);
        else if (verNombres)
            printf("%s\n", nombres[i]);
        else
            printf("%s\n", logins[i]);
    }
    return 0;
}

static int cmdFecha(int n, char *tr[])
{
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char fecha[32], hora[32];
    int verFecha = 1, verHora = 1;

    if (n > 1) {
        if (strcmp(tr[1], "-d") == 0)
            verHora = 0;
        else if (strcmp(tr[1], "-t") == 0)
            verFecha = 0;
        else {
            printf("Opcion no valida: %s\n", tr[1]);
            return 0;
        }
    }
    if (t == NULL) {
        printf("Imposible obtener la fecha\n");
        return 0;
    }
    strftime(fecha, sizeof(fecha), "%d/%m/%Y", t);
    strftime(hora, sizeof(hora), "%H:%M:%S", t);

    if (verFecha)
        printf("%s\n", fecha);
    if (verHora)
        printf("%s\n", hora);
    return 0;
}

static int cmdSalir(int n, char *tr[])
{
    (void) n; (void) tr;
    return 1;
}

static int cmdDup(int n, char *tr[])
{
    int df, nuevo, modo;
    NodoFichero *original;
    char nombre[PATH_MAX];

    if (n < 2) {
        listarFicherosAbiertos();
        return 0;
    }
    if (leerDescriptor(tr[1], &df) == -1) {
        printf("Descriptor no valido: %s\n", tr[1]);
        return 0;
    }

    fflush(stdout);
    if ((nuevo = dup(df)) == -1) {
        errorSistema("duplicar el descriptor", tr[1]);
        return 0;
    }

    if ((original = buscarFichero(df)) != NULL)
        snprintf(nombre, sizeof(nombre), "%s", original->nombre);
    else
        nombreDescriptor(df, nombre, sizeof(nombre));

    modo = fcntl(nuevo, F_GETFL);
    if (original != NULL)
        modo |= original->modo & (O_CREAT | O_EXCL | O_TRUNC);

    if (insertarFichero(nuevo, modo, nombre) == -1) {
        printf("Imposible anadir el duplicado a la lista: memoria insuficiente\n");
        close(nuevo);
        return 0;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", nuevo);
    return 0;
}

/* Marcador para los comandos que aún no están implementados */
static int cmdNoImplementado(int n, char *tr[])
{
    (void) n;
    printf("%s: comando no implementado todavia\n", tr[0]);
    return 0;
}

/* ======================================================================
 *  INTÉRPRETE
 * ====================================================================== */

typedef struct {
    const char *nombre;
    int (*funcion)(int, char *[]);
} Comando;

static const Comando COMANDOS[] = {
    {"authors",  cmdAutores},
    {"date",     cmdFecha},
    {"exit",     cmdSalir},
    {"bye",      cmdSalir},
    {"pid",      cmdNoImplementado},
    {"sysinfo",  cmdNoImplementado},
    {"help",     cmdNoImplementado},
    {"chdir",    cmdNoImplementado},
    {"open",     cmdNoImplementado},
    {"close",    cmdNoImplementado},
    {"listopen", cmdNoImplementado},
    {"dup",      cmdDup},
    {"lseek",    cmdNoImplementado},
    {"readstr",  cmdNoImplementado},
    {"writestr", cmdNoImplementado},
    {"makefile", cmdNoImplementado},
    {"makedir",  cmdNoImplementado},
    {"delete",   cmdNoImplementado},
    {"deltree",  cmdNoImplementado},
    {"listfile", cmdNoImplementado},
    {"list",     cmdNoImplementado},
};

#define NUM_COMANDOS ((int)(sizeof(COMANDOS) / sizeof(COMANDOS[0])))

/* Busca el comando en la tabla y lo ejecuta. Devuelve 1 si hay que salir */
static int procesarComando(int n, char *tr[])
{
    int i;

    for (i = 0; i < NUM_COMANDOS; i++)
        if (strcmp(tr[0], COMANDOS[i].nombre) == 0)
            return COMANDOS[i].funcion(n, tr);

    printf("%s: comando no encontrado\n", tr[0]);
    return 0;
}

int main(void)
{
    char entrada[MAXENTRADA];
    char *trozos[MAXTROZOS];
    int n, salir = 0;

    cargarDescriptoresHeredados();

    while (!salir) {
        printf(PROMPT);
        fflush(stdout);
        if (fgets(entrada, sizeof(entrada), stdin) == NULL) {
            printf("\n");
            break;                      /* fin de la entrada (Ctrl-D) */
        }
        if ((n = trocearCadena(entrada, trozos)) == 0)
            continue;                   /* línea vacía */
        salir = procesarComando(n, trozos);
        fflush(stdout);
    }

    borrarListaFicheros();
    return 0;
}
