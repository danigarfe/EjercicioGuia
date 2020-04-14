#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <mysql/mysql.h>
#include <pthread.h>
#include <netdb.h>


typedef struct {
	char nombre [20];
	int socket;
} Conectado;
typedef struct {
	Conectado conectados [100];
	int num;
} ListaConectados;

typedef struct{
	int socket;
	ListaConectados miLista;
	MYSQL *conn;
} Argumento;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int PonSocket (ListaConectados *milista, int socket){
	if(milista->num<100){
		milista->conectados[milista->num].socket = socket;
		milista->num++;
		return 0;
	}
	else
	   return -1;
}
void PonNombre (ListaConectados *milista, char nombre[30], int socket){
	int encontrado = 0;
	for(int i = 0; encontrado == 0 && i<100; i++){
	if(milista->conectados[i].socket == socket){
		encontrado = 1;
		strcpy(milista->conectados[i].nombre, nombre);
	}
	}
}
int DamePosicion (ListaConectados *lista, char nombre[20]){
	//Devuelve la posicin en la lista o -1 si no estï¿½ en la lista
	int i=0;
	int encontrado = 0;
	while ((i<lista->num) && !encontrado)
	{
		if (strcmp(lista->conectados[i].nombre, nombre) == 0)
			encontrado=1;
		if (!encontrado)
			i=i+1;
	}
	if (encontrado)
		return i;
	else
		return -1;
}

int Eliminar (ListaConectados *lista, char nombre[20]){
	int pos = DamePosicion (lista, nombre);
	if (pos == -1)
		return -1;
	else {
		int i;
		for (i=pos; i < lista->num-1; i++)
		{
			lista->conectados [i] = lista->conectados[i+1];
			//strcpy (lista->conectados[i].nombre, lista->conectados[i+1].nombre);
			//lista->conectados[i].socket =lista->conectados[i+1].socket;
		}
		lista->num--;
		return 0;
	}
}  //Retorna 0 si elimina y -1 si ese usuario no estï¿½ en la lista
void DameConectados (ListaConectados *lista, char conectados [300]){
	// Pone en conectados los nombres de todos los conectados separados
	// por /. Primero pone el numero de conectados. Ejemplo:
	// "3/Juan/Maria/Pedro"
	sprintf (conectados,"%d/", lista->num);
	int i;
	for (i=0; i< lista->num; i++){
		sprintf (conectados, "%s%s,", conectados, lista->conectados[i].nombre);
	}
	conectados[strlen(conectados)-1]='\0';
}
void *AtenderCliente (Argumento *arg){
	int sock_conn=arg->socket;
	int err;
	// Estructura especial para almacenar resultados de consultas
	MYSQL_RES *resultado;
	MYSQL_ROW row;
	
	//int socket_conn = * (int *) socket;
	
	char peticion[512];
	char respuesta[512];
	int ret;
	
	
	
	int i=1;
	int terminar=0;
	while(terminar==0)
	{
		//sock_conn es el socket que usaremos para este cliente
		// Ahora recibimos la peticiÃ³n
		ret=read(sock_conn,peticion, sizeof(peticion));
		printf("Recibido\n");
		
		// Tenemos que aÃ±adirle la marca de fin de string
		// para que no escriba lo que hay despues en el buffer
		peticion[ret]='\0';
		printf("Peticion: %s\n",peticion);
		
		char *p = strtok(peticion, "/");
		int codigo = atoi(p);
		char nombre[30];
		char contrasena[30];
		
		if (codigo == 0) //Desconectarse
		{
			terminar = 1;
			strcpy(respuesta, "DESCONECTA");
			printf("Conexión terminada.\n");
			
			p = strtok(NULL,"/"); //Cogemos el nombre para eliminarlo de la lista de conectados
			strcpy(nombre,p);
			pthread_mutex_lock(&mutex);
			int eliminar = Eliminar(&arg->miLista, nombre);
			pthread_mutex_unlock(&mutex);
			if (eliminar == -1)
				printf("No estaba conectado\n");
			if (eliminar == 0)
				printf("Eliminado correctamente\n");
			
		}
		
		if (codigo==1) //Registrarse 	1/Nombre/Contraseï¿½a
		{
			int igual=0;
			p = strtok(NULL,"/");
			strcpy(nombre,p);
			char consulta [500];
			
			strcpy(consulta, "SELECT nombre FROM Jugador WHERE Jugador.nombre = '");
			strcat(consulta, nombre);
			strcat(consulta, "';");
			
			err=mysql_query(arg->conn, consulta);
			if (err != 0)
			{
				sprintf(respuesta, "ERROR");
			}
			else
			{
				resultado = mysql_store_result(arg->conn);
				row = mysql_fetch_row(resultado);
				
				if (row == NULL)
				{
					p = strtok(NULL, "/");
					strcpy(contrasena,p);
					
					
					err=mysql_query(arg->conn, "SELECT COUNT(*) FROM Jugador;");
					resultado = mysql_store_result(arg->conn);
					row = mysql_fetch_row(resultado);
					
					
					char consulta[500];
					
					strcpy(consulta, "INSERT INTO Jugador VALUES("); //sustituir por un sprint
					strcat(consulta, row[0]);
					strcat(consulta, ",'");
					strcat(consulta, nombre);
					strcat(consulta, "','");
					strcat(consulta, contrasena);
					strcat(consulta, "',0);");
					
					err=mysql_query(arg->conn, consulta);
					
					
					sprintf(respuesta, "REGISTRADO OK");
				}
				
				else
				{
					sprintf(respuesta,"NOMBRE EN USO");
				}
			}
		}
		
		else if (codigo==2) //Loguearse		2/Nombre/Contraseï¿½a
		{
			p = strtok(NULL, "/");
			strcpy(nombre,p);
			
			p = strtok(NULL,"/");
			strcpy(contrasena,p);
			char consulta[500];
			
			strcpy(consulta, "SELECT * FROM Jugador WHERE Jugador.nombre = '");
			strcat(consulta,nombre);
			strcat(consulta, "' AND Jugador.contraseña = '");
			strcat(consulta, contrasena);
			strcat(consulta, "';");
			
			err=mysql_query(arg->conn, consulta);
			resultado = mysql_store_result(arg->conn);
			row = mysql_fetch_row(resultado); //AQUI EL ERROR
			if(row==NULL)
			{
				sprintf(respuesta, "NO ENTRA");
			}
			else{
				sprintf(respuesta, "ENTRA");
				printf("%s conectado.\n", nombre);
				pthread_mutex_lock(&mutex);
				PonNombre(&arg->miLista, nombre, sock_conn);
				pthread_mutex_unlock(&mutex);
			}
		}
		
		else if (codigo==3)	//Numero de partidas ganadas por un jugador	3/Nombre
		{
			p=strtok(NULL,"/");
			strcpy(nombre,p);
			char consulta [500];
			
			strcpy(consulta,"SELECT COUNT(nombreganador) FROM Partida WHERE nombreganador = '");
			strcat(consulta, nombre);
			strcat(consulta,"';");
			// hacemos la consulta
			err=mysql_query(arg->conn, consulta);
			if (err!=0) {
				sprintf(respuesta,"ERROR");
			}
			else
			{
				resultado = mysql_store_result(arg->conn);
				row = mysql_fetch_row(resultado);
				if (row == NULL)
					sprintf(respuesta,"NO EXISTE");
				else
				{
					int result = atoi(row[0]);
					sprintf(respuesta,"%d",result);
				}
			}
		}
		
		else if (codigo==4)	//Nombre del ganador de una partida en concreto		4/idPartida
		{
			char idPartida [20];
			p=strtok(NULL,"/");
			strcpy(idPartida,p);
			char consulta [500];
			
			strcpy(consulta,"SELECT Jugador.nombre FROM Jugador, Partida WHERE Partida.id=");
			strcat(consulta, idPartida);
			strcat(consulta," AND Jugador.nombre=Partida.nombreganador;");
			err=mysql_query(arg->conn, consulta);
			if (err!=0) {
				sprintf(respuesta,"ERROR");
			}
			else
			{
				resultado = mysql_store_result(arg->conn);
				row = mysql_fetch_row(resultado);
				if (row == NULL)
					sprintf(respuesta,"NO EXISTE");
				else
					sprintf(respuesta,"%s",row[0]);
			}
		}
		
		else if (codigo==5)			//Numero de goles de un jugador		5/Nombre
		{
			int golesresultado;
			p=strtok(NULL,"/");
			strcpy(nombre,p);
			
			char consulta [500];
			
			strcpy(consulta,"SELECT SUM(RelacionJP.goles) FROM Jugador, RelacionJP WHERE Jugador.nombre='");
			strcat(consulta, nombre);
			strcat(consulta,"' AND Jugador.id=RelacionJP.id_J;");
			
			
			err=mysql_query(arg->conn, consulta);
			if (err!=0)
			{
				sprintf(respuesta, "ERROR");
			}
			else
			{
				resultado = mysql_store_result(arg->conn);
				
				row = mysql_fetch_row(resultado);
				
				if (row == NULL)
					sprintf(respuesta,"NO EXISTE");
				else
				{
					// la columna 1 contiene una palabra que son los goles
					// la convertimos a entero
					golesresultado= atoi(row[0]);
					sprintf(respuesta,"%d",golesresultado);
				}
			}
		}
		
		else if (codigo==6)		//DameConectados	6/
		{
			char conectados[300];
			DameConectados(&arg->miLista,conectados);
			sprintf(respuesta,"%s",conectados); //Devolvemos nombres de conectados separados por comas.
		}
		
		write(sock_conn,respuesta, strlen(respuesta));
		
		
		// Se acabo el servicio para este cliente
	}
	close(sock_conn);
}
int main(int argc, char *argv[]){
	
	int err;
	// Estructura especial para almacenar resultados de consultas
	Argumento arg;
	arg.miLista.num=0;
	
	//Creamos una conexion al servidor MYSQL
	arg.conn = mysql_init(NULL);
	if (arg.conn==NULL) {
		printf("Error al crear la conexion: %u %s\n",
			   mysql_errno(arg.conn), mysql_error(arg.conn));
		exit(1);
	}
	//inicializar la conexion
	arg.conn = mysql_real_connect(arg.conn, "localhost", "root", "mysql", "BBDD",0, NULL, 0);
	if (arg.conn==NULL) {
		printf("Error al inicializar la conexion: %u %s\n",
			   mysql_errno(arg.conn), mysql_error(arg.conn));
		exit(1);
	}
	
	//---------------------------------------------------------------------------------
	
	int sock_conn, sock_listen;
	struct sockaddr_in serv_adr;
	
	
	// INICIALITZACIONS
	// Obrim el socket
	if ((sock_listen=socket(AF_INET,SOCK_STREAM,0))< 0)
	{
		printf("Error creant socket");
	}
	// Fem el bind al port
	
	
	memset(&serv_adr, 0, sizeof(serv_adr));//inicialitza a zero serv_addr
	serv_adr.sin_family = AF_INET;
	
	// asocia el socket a cualquiera de las IP de la m?quina.
	//htonl formatea el numero que recibe al formato necesario
	serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
	// establecemos el puerto de escucha
	
	
	
	
	
	serv_adr.sin_port = htons(9060); //AQUï¿½ EL PUERTO
	
	
	
	
	
	
	
	
	
	
	if (bind(sock_listen, (struct sockaddr *) &serv_adr, sizeof(serv_adr)) < 0)
		printf("Error al bind");
	
	if (listen(sock_listen, 3) < 0)
		printf("Error en el Listen");
	pthread_t thread;
	int errorlista;
	// Bucle para atender a 5 clientes
	for (;;){
		printf ("Escuchando\n");
		sock_conn = accept(sock_listen, NULL, NULL);
		pthread_mutex_lock(&mutex);
		arg.socket=sock_conn;
		errorlista = PonSocket(&arg.miLista, arg.socket);
		pthread_mutex_unlock(&mutex);
		if(errorlista == 0){
		printf("He recibido conexion\n");
		printf("Socket: %d\n", arg.socket);
		//sock_conn es el socket que usaremos para este cliente
		
		// Crear thead y decirle lo que tiene que hacer
		
		pthread_create (&thread, NULL, AtenderCliente, &arg);
		}
		else if(errorlista == -1){
			printf("Lista de conectados llena.");
		}
	}
	
}


