#ifndef SERVER_DATA
#define SERVER_DATA

// сообщения 
#define MAX_MESS_LEN 256

typedef struct {
char message[MAX_MESS_LEN + 1];
} message;


// Функции инициализации и завершения
int initialize(const int new_database);
void end(void);

// Две функции для простого извлечения данных


// Две функции для добавления данных


// Две функции для удаления данных



#endif //SERVER_DATA