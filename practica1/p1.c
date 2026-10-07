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

#define MAXLINEA 1024
#define MAXTROZOS 512

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

int TrocearCadena(char *cadena, char *trozos[])
{
    int i = 1;

    if ((trozos[0] = strtok(cadena, " \n\t")) == NULL)
        return 0;
    while (i < MAXTROZOS - 1 && (trozos[i] = strtok(NULL, " \n\t")) != NULL)
        i++;
    trozos[i] = NULL;
    return i;
}

// para los errores de llamadas al sistema, objeto puede ser NULL
void ErrorSis(char *accion, char *objeto)
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


int Cmd_authors(int n, char *tr[])
{
    char *nombres[] = {"Francisco Martínez Rubido", "Román Casas Riveira"};
    char *logins[] = {"f.martinezr@udc.es", "r.casas@udc.es"};
    int nom = 1, log = 1, i;

    if (n > 1) {
        if (!strcmp(tr[1], "-l"))
            nom = 0;
        else if (!strcmp(tr[1], "-n"))
            log = 0;
        else {
            printf("Opcion no valida: %s\n", tr[1]);
            return 0;
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
    return 0;
}

int Cmd_date(int n, char *tr[])
{
    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char buf[32];
    int fecha = 1, hora = 1;

    if (n > 1) {
        if (!strcmp(tr[1], "-d"))
            hora = 0;
        else if (!strcmp(tr[1], "-t"))
            fecha = 0;
        else {
            printf("Opcion no valida: %s\n", tr[1]);
            return 0;
        }
    }
    if (t == NULL) {
        printf("Imposible obtener la fecha\n");
        return 0;
    }

    if (fecha) {
        strftime(buf, sizeof(buf), "%d/%m/%Y", t);
        printf("%s\n", buf);
    }
    if (hora) {
        strftime(buf, sizeof(buf), "%H:%M:%S", t);
        printf("%s\n", buf);
    }
    return 0;
}

int Cmd_exit(int n, char *tr[])
{
    return 1;
}

int Cmd_open(int n, char *tr[])
{
    int i, df, modo = 0;

    if (n == 1) {
        ListarAbiertos();
        return 0;
    }

    for (i = 2; i < n; i++) {
        if (!strcmp(tr[i], "cr"))
            modo |= O_CREAT;
        else if (!strcmp(tr[i], "ex"))
            modo |= O_EXCL;
        else if (!strcmp(tr[i], "ro"))
            modo |= O_RDONLY;
        else if (!strcmp(tr[i], "wo"))
            modo |= O_WRONLY;
        else if (!strcmp(tr[i], "rw"))
            modo |= O_RDWR;
        else if (!strcmp(tr[i], "ap"))
            modo |= O_APPEND;
        else if (!strcmp(tr[i], "tr"))
            modo |= O_TRUNC;
        else {
            printf("Modo no valido: %s\n", tr[i]);
            return 0;
        }
    }

    df = open(tr[1], modo, 0777);
    if (df == -1) {
        ErrorSis("abrir", tr[1]);
        return 0;
    }
    if (AnadirFichero(df, modo, tr[1]) == -1) {
        printf("Imposible anadir %s a la lista: memoria insuficiente\n", tr[1]);
        close(df);
        return 0;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", df);
    return 0;
}

int Cmd_close(int n, char *tr[])
{
    int i, df;
    char *arg = NULL;

    // vale "close df", "close df -f" y "close -f df"
    // -f de momento no hace nada distinto porque aun no hay mapeos
    for (i = 1; i < n; i++) {
        if (!strcmp(tr[i], "-f"))
            continue;
        if (arg != NULL || LeerDf(tr[i], &df) == -1) {
            printf("Uso: close df [-f]\n");
            return 0;
        }
        arg = tr[i];
    }
    if (arg == NULL) {
        ListarAbiertos();
        return 0;
    }

    fflush(stdout);   // por si cerramos la salida estandar
    if (close(df) == -1) {
        ErrorSis("cerrar el descriptor", arg);
        return 0;
    }
    EliminarFichero(df);
    return 0;
}

int Cmd_dup(int n, char *tr[])
{
    int df, nuevo, modo;
    char nombre[PATH_MAX];
    struct fichero *orig;

    if (n < 2) {
        ListarAbiertos();
        return 0;
    }

    if (LeerDf(tr[1], &df) == -1) {
        printf("Descriptor no valido: %s\n", tr[1]);
        return 0;
    }

    fflush(stdout);
    if ((nuevo = dup(df)) == -1) {
        ErrorSis("duplicar el descriptor", tr[1]);
        return 0;
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
        return 0;
    }
    printf("Anadida entrada %d a la tabla ficheros abiertos\n", nuevo);
    return 0;
}

int Cmd_lseek(int n, char *tr[])
{
    int df, ref;
    off_t pos, res;
    char *fin;

    if (n != 4) {
        printf("Uso: lseek df pos SEEK_SET|SEEK_CUR|SEEK_END\n");
        return 0;
    }
    if (LeerDf(tr[1], &df) == -1) {
        printf("Descriptor no valido: %s\n", tr[1]);
        return 0;
    }

    errno = 0;
    pos = strtoll(tr[2], &fin, 10);
    if (errno != 0 || *fin != '\0') {
        printf("Posicion no valida: %s\n", tr[2]);
        return 0;
    }

    if (!strcmp(tr[3], "SEEK_SET"))
        ref = SEEK_SET;
    else if (!strcmp(tr[3], "SEEK_CUR"))
        ref = SEEK_CUR;
    else if (!strcmp(tr[3], "SEEK_END"))
        ref = SEEK_END;
    else {
        printf("Referencia no valida: %s (usar SEEK_SET, SEEK_CUR o SEEK_END)\n", tr[3]);
        return 0;
    }

    res = lseek(df, pos, ref);
    if (res == -1)
        ErrorSis("posicionar el descriptor", tr[1]);
    else
        printf("Nuevo offset: %lld\n", (long long) res);
    return 0;
}

int Cmd_readstr(int n, char *tr[])
{
    int df;
    long cont;
    ssize_t leidos;
    char *fin, *buf;

    if (n != 3) {
        printf("Uso: readstr df cont\n");
        return 0;
    }
    if (LeerDf(tr[1], &df) == -1) {
        printf("Descriptor no valido: %s\n", tr[1]);
        return 0;
    }

    errno = 0;
    cont = strtol(tr[2], &fin, 10);
    if (errno != 0 || *fin != '\0' || cont < 0 || cont >= SSIZE_MAX) {
        printf("Numero de bytes no valido: %s\n", tr[2]);
        return 0;
    }

    buf = malloc(cont + 1);   // +1 para el '\0'
    if (buf == NULL) {
        printf("Imposible reservar %ld bytes\n", cont);
        return 0;
    }

    leidos = read(df, buf, cont);
    if (leidos == -1) {
        ErrorSis("leer del descriptor", tr[1]);
        free(buf);
        return 0;
    }
    buf[leidos] = '\0';
    printf("%s\n", buf);
    printf("Leidos %ld bytes del descriptor %d\n", (long) leidos, df);
    free(buf);
    return 0;
}

int Cmd_writestr(int n, char *tr[])
{
    int df, i, tam = 0;
    ssize_t escritos;
    char *cad;

    if (n < 3) {
        printf("Uso: writestr df str\n");
        return 0;
    }
    if (LeerDf(tr[1], &df) == -1) {
        printf("Descriptor no valido: %s\n", tr[1]);
        return 0;
    }

    // strtok separa por espacios, asi que hay que volver a juntar los trozos
    for (i = 2; i < n; i++)
        tam += strlen(tr[i]) + 1;
    cad = malloc(tam);
    if (cad == NULL) {
        printf("Imposible reservar memoria\n");
        return 0;
    }
    strcpy(cad, tr[2]);
    for (i = 3; i < n; i++) {
        strcat(cad, " ");
        strcat(cad, tr[i]);
    }

    escritos = write(df, cad, strlen(cad));
    if (escritos == -1)
        ErrorSis("escribir en el descriptor", tr[1]);
    else
        printf("Escritos %ld bytes en el descriptor %d\n", (long) escritos, df);
    free(cad);
    return 0;
}

// TODO: ir quitando estos segun los hagamos
int Cmd_pendiente(int n, char *tr[])
{
    printf("%s: comando no implementado todavia\n", tr[0]);
    return 0;
}


struct cmd {
    char *nombre;
    int (*func)(int, char **);
};

struct cmd comandos[] = {
    {"authors", Cmd_authors},
    {"date", Cmd_date},
    {"exit", Cmd_exit},
    {"bye", Cmd_exit},
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
    {"listfile", Cmd_pendiente},
    {"list", Cmd_pendiente},
    {NULL, NULL}
};

int ProcesarComando(int n, char *tr[])
{
    int i;

    for (i = 0; comandos[i].nombre != NULL; i++)
        if (!strcmp(tr[0], comandos[i].nombre))
            return comandos[i].func(n, tr);

    printf("%s: comando no encontrado\n", tr[0]);
    return 0;
}

int main()
{
    char linea[MAXLINEA];
    char *trozos[MAXTROZOS];
    int n, terminado = 0;

    CargarHeredados();

    while (!terminado) {
        printf("-> ");
        fflush(stdout);
        if (fgets(linea, MAXLINEA, stdin) == NULL) {   // ctrl-D
            printf("\n");
            break;
        }
        n = TrocearCadena(linea, trozos);
        if (n == 0)
            continue;
        terminado = ProcesarComando(n, trozos);
        fflush(stdout);
    }

    BorrarLista();
    return 0;
}