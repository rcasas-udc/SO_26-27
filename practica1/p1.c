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

#define MAXENTRADA 1024
#define MAXTROZOS  512
#define PROMPT     "-> "


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
    {"dup",      cmdNoImplementado},
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

    return 0;
}
