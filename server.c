
//*=====================================================A L T O======================================================*/
//|														    												    	 |
//| -A partir de aqui­ podremos encontrar las funciones necesarias para las funciones de base de datos y sockets.     |
//|														     														 |
//| -¿Como se distribuye? Bien primero vienen las funciones de base de datos, encontraras antes de cada funcion      |
//| una breve explicacion de que hacen, dentro hay mas comentarios explicando paso por paso que hacen.               |
//|														    														 |
//| -Si respetamos los espacios dedicados para cada funcion, nos entenderemos mejor, estaremos mas coordinados,      |
//| sera mas facil coordinarnos y encontrar los errores.							    							 |
//|														    														 |
//| -En la parte de base de datos, tenemos LogIN,SignIN,PuntuacionTotal,TiempoJugado,VecesGanadas{1,2,3,4,5,...}     |
//| ademas de funciones extras que son una dedicada a la consulta otras dedicadas para cerrar y abrir la base de     |
//| datos. Si quereis añadir funciones de juego(Seleccionar,etc) por favor añadirlas a continuacion de la anterior   |
//| nunca despues de las especaficas como por ejemplo Abrir Cerrar Consultar.					     				 |
//|														    														 |
//| -Seguir lo mismo para el servidor.										    									 |
//|														    														 |
//| -A poder ser, no mezclar cosas de juego como modificar strings, añadir jugadores a la lista, etc. Estas funciones|
//| estan en la parte superior del MAIN, por favor tembien pido respetar esto. Si os cuesta mucho seguir, vosotros   |
//| hacedlo sencillo, yo lo arreglo y lo pongo bonito.															     |
//*==================================================================================================================*/

#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <mysql.h>
#include <pthread.h>


//-,-,-,-,-,-,-,-, D E F I N E S ,-,-,-,-,-,-,-,-,-,-,-,
#define TRUE 1
#define FALSE 0
#define MAX_NUM_USUARIOS 100
#define PORT 9100

//-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,-,

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx E S T R U C T U R A   D E   P A R T I D A xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
//Este espacio es para crear la lista de partidas, jugadores conectados, etc.
typedef struct{
	char nombre[20];
	int socket;
}Usuario;

typedef struct{
	Usuario LUsuarios[MAX_NUM_USUARIOS];
	int num;
}TUsuarios;

//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx

//---------V A R I A B L E S   G L O B A L E S---------
MYSQL *mysql_conn;
TUsuarios usuarios[MAX_NUM_USUARIOS];
struct sockaddr_in serv_adr;
int serverSocket;
int sock_atendedidos[MAX_NUM_USUARIOS];
pthread_t thread[MAX_NUM_USUARIOS];
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
MYSQL_RES *res;
MYSQL_ROW *row;
//-----------------------------------------------------

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~F U N C I O N E S   D E   S O C K E T  Y  T H R E A D S~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// AquÃ­ tendrÃ­an que haber funciones de procesado de strings, puntuaciones, nÃºmeros, aÃ±adir jugadores, aÃ±adir partidas a una lista, etc.
void *ThreadExecute (void *socket){
	char buff[512];
	char output[512];
	int sock_att, start = TRUE, ret;
	int *s;
	s = (int *)socket;
	sock_att = *s;
	while (start){
		ret = read(sock_att, buff, sizeof(buff));
		buff[ret] = '\0';
		
		printf("Recibida orden de Socket %d.\n", sock_att);
		printf("Pide: \n");
		puts(buff);
		
		pthread_mutex_lock(&lock);
		EjecutarCodigo(buff ,sock_att, output, &start);
		pthread_mutex_unlock(&lock);
		
		if(start){
			printf("Enviamos: %s\n", output);
			write(sock_att, output, strlen(output));
		}
	}
}

void InitServer(){
	//CONFIGURACION DEL SERVIDOR
	printf("[Iniciando Servidor...]\n");
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	printf("Habilitamos serverSocket.\n");
	
	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family = AF_INET;
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_adr.sin_port = htons(PORT);
	printf("Configuración del server lista, escuchando en puerto: %d\n", PORT);
}

void InitBind(){
	//CONFIGURACION DEL BIND
	printf("[Iniciando Bind...]\n");
	if (bind(serverSocket, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0){
		printf ("Error al bind\n");
		exit(1);
	}
	else{
		printf("Bind creado correctamente.\n");
		
		//Comprobamos que el servidor este escuchando
		if (listen(serverSocket, 2) < 0){
			printf("Error en el Listen");
			exit(1);
		}
		else
			printf("serverSocket funcionando correctamente.\n[Todo OK.]\n");
	}
}

//....................................................... M A I N .......................................................

int main(int argc, char *argv[])
{
	InitServer();
	InitBind();
	IniciarBBDD();
	
	int attendSocket, i = 0;
	// Atender las peticiones
	for( ; ; ){
		printf ("Escuchando\n");
		attendSocket= accept(serverSocket, NULL, NULL);
		printf ("He recibido conexion\n");
		//attendSockeet es el socket que usaremos para un cliente
		
		sock_atendedidos[i] = attendSocket;
		
		pthread_create(&thread[i], NULL, ThreadExecute, &sock_atendedidos[i]); //Thread_que_toque_usar, NULL, Nombre_de_la_funcion_del_thread, socket_del cliente
		i++;
	}
	
	CerrarBBDD();
}
//.......................................................................................................................

//vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv B A S E   D E   D A T O S vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv

int LogIN(char input[512], char output[512]){
	char nombre1[20];
	char contra1[20];
	char consulta[512];

	//Obtenir l'informaciÃ³ que ens donen:
	char *p = strtok(input, " "); //Nombre de usuario
	strcpy(nombre1, p);
	p = strtok(NULL, " "); //Contraseña
	strcpy(contra1, p);
	
	//Mirem si existeix tal usuari:
	if (EncontrarJugador(nombre1)){
		//CreaciÃ³ de la consulta:
		sprintf(consulta, "SELECT usuario, contrasena FROM jugador WHERE usuario = '%s';", nombre1);
		
		//Obtenemos el resultado de la consulta.
		ConsultaBBDD(consulta); //Ya se ha asignado el valor de la consulta al res.
	
		row = mysql_fetch_row(res);
		// !algo significa que esperamos que devuelva 0
		if (!strcmp(row[0], nombre1)){
			if(!strcmp(row[1], contra1)){
				strcpy(output, "1/1"); //Existe, retornamos 1.
				//falta afegir a la lista de connectats. retornamos 1.
				return TRUE; //retornamos 1 para que lo ponga en la lista de conectados.
			}
			else{
				strcpy(output, "1/0");
				return FALSE;
			}
		}
		else{
				strcpy(output, "1/0");
				return FALSE;
			}
		}
	else{
		strcpy(output, "1/0"); //No existe, retornamos 0.
		return FALSE;
	}
}

int SignUP(char input[512], char output[512]){
	char nombre[20];
	char contra[20];
	char consulta[512];
	int ID, existe = TRUE;
	//Obtener la informaciÃ³n del input.
	sscanf(input,"%s %s", nombre, contra);
	
	ID = rand();
	if(ExisteID(ID)){ //Si existe la ID, hay que cambiarla
		while(existe){
			ID = rand();
			existe = ExisteID(ID);
		}
		existe = TRUE; //Como la ID random ahora no existe podemos decir que la ID nueva esta libre con lo cual puede existir.
	}
	if(existe){ //Si la ID está libre, significa que puede existir
		if(!EncontrarJugador(nombre)){
			sprintf(consulta, "INSERT INTO jugador (id, usuario, contrasena) VALUES(%d ,'%s', '%s')", ID ,nombre, contra);
			ConsultaBBDD(consulta); //Registrado

			if(EncontrarJugador(nombre)){
				strcpy(output, "2/1");
				return TRUE;
			}
			else{
				strcpy(output, "2/0");
				return FALSE;
			}
		}
	}
}

int PuntosTotales(char input[512], char output[512]){
	char usuario[20];
	char consulta[512];

	//Queremos obtener el usuario del cual recoger la puntuaciÃ³n total:
	sscanf(input, "%s", usuario);
	if(EncontrarJugador(usuario)){
		//Queremos recoger el valor de la consulta:
		sprintf(consulta, "SELECT SUM(relacion.puntuacion) FROM jugador, relacion, partidas WHERE relacion.idjug = jugador.id AND jugador.usuario ='%s';", usuario);
		ConsultaBBDD(consulta);
		
		row = mysql_fetch_row(res);
		if(row[0] != NULL){
			printf("%d\n", atoi(row[0]));
			
			int result = atoi(row[0]);
			
			sprintf(output, "3/%d", result);
			return TRUE;
		}
		else{
			sprintf(output, "3/0");
			return FALSE;
		}
	}
	else{
		sprintf(output, "3/0");
		return FALSE;
	}	
}

int TiempoTotalJugado(char input[512], char output[512]){
	char usuario[20];
	char consulta[512];
	
	sscanf(input, "%s", usuario);
	if(EncontrarJugador(usuario)){
		sprintf(consulta, "SELECT SUM(partidas.duracion) FROM partidas, jugador, relacion WHERE partidas.id = relacion.idpartida AND relacion.idjug = jugador.id AND jugador.usuario = '%s';", usuario);
		ConsultaBBDD(consulta);

		row = mysql_fetch_row(res);

		if(row[0] == NULL){
			strcpy(output, "4/0");
			return FALSE;
		}
		else{
			int result = atoi(row[0]);
			sprintf(output, "4/%d", result);
			return TRUE;
		}
	}
	else{
		strcpy(output, "4/0");
		return FALSE;
	}
}

int PartidasGanadasVS(char input[512], char output[512]){
	char usuario[20];
	char consulta[512];

	sscanf(input, "%s", usuario);
	if(EncontrarJugador(usuario)){
		sprintf(consulta, "SELECT COUNT(ganador) FROM partidas WHERE ganador = '%s';", usuario);
		ConsultaBBDD(consulta);

		row = mysql_fetch_row(res);
		
		if(row[0] == NULL){
			strcpy(output, "5/0");
			return FALSE;
		}
		else{
			int result = atoi(row[0]);
			sprintf(output, "5/%d", result);
			return TRUE;
		}
	}
	else{
		strcpy(output, "5/0");
		return FALSE;
	}
}

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.ESPECiFICAS.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.
void IniciarBBDD(){
	//CONFIGURAMOS LA CONEXION BASEDATOS CON SERVIDOR C
	int err;
	mysql_conn = mysql_init(NULL);
	if (mysql_conn==NULL) 
	{
		printf ("Error al crear la conexion: %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	//inicializar la conexion, indicando nuestras claves de acceso
	// al servidor de bases de datos (user,pass)
	mysql_conn = mysql_real_connect (mysql_conn, "localhost","root", "mysql", NULL, 0, NULL, 0);
	if (mysql_conn==NULL)
	{
		printf ("Error al inicializar la conexion: %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	
	//indicamos la base de datos con la que queremos trabajar 
	err=mysql_query(mysql_conn, "use juego;");
	if (err!=0)
	{
		printf ("Error al conectar con la base de datos %u %s\n", 
				mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
}

void CerrarBBDD(){
	mysql_close(mysql_conn);
}

void ConsultaBBDD(char consulta[512]){
	int err;
	err = mysql_query(mysql_conn, consulta);
	if(err){
		printf ("Error al crear la conexion: %u %s\n", 
		mysql_errno(mysql_conn), mysql_error(mysql_conn));
		exit (1);
	}
	else{
		res = mysql_store_result(mysql_conn); //Esto se asigna a la variable global MYSQL_RES *res
		printf("Consulta hecha.\n");
	}
}

int CompararCredenciales(char nombre1[20], char nombre2[20], char contra1[20], char contra2[20]){
	if((strcmp(nombre1, nombre2))&&(strcmp(contra1, contra2)))
		return FALSE;
	else
		return TRUE;
}

void ObtenerRegistrados(){
	char consulta[512];
	strcpy(consulta, "SELECT * FROM jugador;");

	ConsultaBBDD(consulta);
}

int EncontrarJugador(char nombre[20]){
	//Queremos encontrar un jugador en la tabla de jugadores.
	char consulta[512];
	
	sprintf(consulta, "SELECT id FROM jugador WHERE usuario = '%s';", nombre);
	
	ConsultaBBDD(consulta);
	row = mysql_fetch_row(res);

	if (row == NULL)
		return FALSE;
	else
		return TRUE;
}

int ExisteID(int ID){
	char consulta[512];
	sprintf(consulta, "SELECT usuario FROM jugador WHERE id = %d;", ID);
	
	ConsultaBBDD(consulta);
	row = mysql_fetch_row(res);
	
	if(row == NULL)
		return FALSE; //Si no existe la ID, devolvemos 0
	else
		return TRUE; //Si existe, devolvemos 1
}

//.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.-.

//,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,, F U N C I O N E S   D E   J U E G O ,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,

void EjecutarCodigo(char buff[512], int socket, char output[512], int *start)
{ 
	char input[512];
	int codigo;
	codigo = DarCodigo(buff, input);
	
	if (codigo == 1){
		if(LogIN(input, output))
			printf("Socket %d se ha connectado.\n", socket);
		else
			printf("Hubo algún problema.\n");
	}
	else if (codigo == 2){
		if(SignUP(input, output)){
			printf("Socket %d se ha registrado.\n", socket);
		}
		else
			printf("Hubo algún problema.\n");
	}
	else if (codigo == 3){
		if(PuntosTotales(input, output)>0)
			printf("Success.\n");
		else
			printf("Hubo algún problema.\n");
	}
	else if (codigo == 4){
		if(TiempoTotalJugado(input, output)>0)
			printf("Success.\n");
		else
			printf("Hubo algún problema.\n");
	}
	else if (codigo == 5){
		if(PartidasGanadasVS(input, output) != 0)
			printf("Success.\n");
		else
			printf("Hubo algún problema.\n");
	}
	if(codigo == 0){
		*start = FALSE;
		printf("Desconexión Socket %d\n", socket);
	}
}

int DarCodigo(char buff[512],char input[512]){
	char *p =strtok(buff,"/");
	int codigo = atoi(p);
	if(codigo != 0){ //Si el codi es 0 no fa falta seguir troçejant
		p = strtok(NULL, "/");
		strcpy(input,p);
	}
	
	return codigo;
}

//,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,
