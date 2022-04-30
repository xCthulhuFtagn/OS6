#ifndef CD_DATA
#define CD_DATA

// Таблица catalog 
#define CAT_CAT_LEN 30
#define CAT_TITLE_LEN 70
#define CAT_TYPE_LEN 30
#define CAT_ARTIST_LEN 70

typedef struct {
char catalog[CAT_CAT_LEN + 1];
char title[CAT_TITLE_LEN + 1];
char type [CAT_TYPE_LEN + 1];
char artist [CAT_ARTIST_LEN + 1];
} cdc_entry;

// Таблица дорожек, по одному элементу на дорожку 
#define TRACK_CAT_LEN CAT_CAT_LEN
#define TRACK_TTEXT_LEN 70

typedef struct {
char catalog[TRACK_CAT_LEN + 1];
int track_no;
char track_txt [TRACK_TTEXT_LEN + 1];
} cdt_entry;

// Функции инициализации и завершения
int database_initialize(const int new_database);
void database_close(void);

// Две функции для простого извлечения данных
cdc_entry get_cdc_entry(const char* cd_catalog_ptr);
cdt_entry get_cdt_entry(const char* cd_catalog_ptr, const int track_no);

// Две функции для добавления данных
int add_cdc_entry(const cdc_entry cd_catalog_to_add);
int add_cdt_entry(const cdt_entry cd_catalog_to_add);

// Две функции для удаления данных
int del_cdc_entry(const char* cd_catalog_ptr);
int del_cdt_entry(const char* cd_catalog_ptr, const int track_no);

// Одна функция поиска 
cdc_entry search_cdc_entry(const char* cd_catalog_ptr, int* first_call_ptr);

#endif //CD_DATA