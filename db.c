#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#ifdef __APPLE__
#include <sys/types.h>
#endif

#ifdef _WIN32
typedef long long ssize_t;
#endif

#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
#define TABLE_MAX_PAGES 100




#define size_of_attribute(Struct, Attribute) sizeof(((Struct*)0) -> Attribute)

typedef struct{
    char *buffer;
    size_t buffer_length;
    size_t input_length;
} InputBuffer;

typedef enum{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommand;

typedef enum{
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    SYNTAX_ERROR
} PrepareResult;

typedef enum{
    STATEMENT_INSERT,
    STATEMENT_SELECT
} StatementType;

typedef enum{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_TABLE_EMPTY
} ExecuteResult;

typedef struct{
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];
} Row;

typedef struct{
    StatementType type;
    Row row_to_insert;
} Statement;

#define ID_SIZE        size_of_attribute(Row, id)
#define USERNAME_SIZE  size_of_attribute(Row, username)
#define EMAIL_SIZE     size_of_attribute(Row, email)
#define ID_OFFSET      0
#define USERNAME_OFFSET (ID_OFFSET + ID_SIZE)
#define EMAIL_OFFSET    (USERNAME_OFFSET + USERNAME_SIZE)

#define PAGE_SIZE 4096
#define ROW_SIZE        (ID_SIZE + USERNAME_SIZE + EMAIL_SIZE)
#define ROWS_PER_PAGE   (PAGE_SIZE / ROW_SIZE)
#define TABLE_MAX_ROWS  (TABLE_MAX_PAGES * ROWS_PER_PAGE)

typedef struct{
    uint32_t number_of_rows;
    void* pages[TABLE_MAX_PAGES];
} Table;

void read_input(InputBuffer *input_buffer);
void print_prompt(void);
void execute_statement(Statement *statement);
void* row_slot(Table* table, uint32_t row_num);
MetaCommand do_meta_command(InputBuffer *input_buffer);
PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement);

InputBuffer *new_input_buffer(void){
    InputBuffer *input_buffer = (InputBuffer*)malloc((sizeof(InputBuffer)));
    input_buffer->buffer = NULL;
    input_buffer->buffer_length = 0;
    input_buffer->input_length = 0;

    return input_buffer;
}

void close_input_buffer(InputBuffer *input_buffer){
    free(input_buffer->buffer);
    free(input_buffer);
}

int main(int argc, char *argv[]){
    InputBuffer *input_buffer = new_input_buffer();
    while(true){
        print_prompt();
        read_input(input_buffer);
        if(input_buffer->buffer[0] == '.'){
            switch(do_meta_command(input_buffer)){
                case(META_COMMAND_SUCCESS):
                    continue;
                case(META_COMMAND_UNRECOGNIZED_COMMAND):
                    printf("Unrecognized command %s \n", input_buffer->buffer);
                    continue;
            }
        }
        Statement statement;
        
        switch(prepare_statement(input_buffer, &statement)){
            case(PREPARE_SUCCESS):
                break;
            case(PREPARE_UNRECOGNIZED_STATEMENT):
                printf("Unrecognized keyword at start of %s ", input_buffer->buffer);
                continue;
        }

        execute_statement(&statement);
        printf("Executed.\n");
    }
}

MetaCommand do_meta_command(InputBuffer *input_buffer){
     if(strcmp(input_buffer->buffer, ".exit") == 0){
            exit(EXIT_SUCCESS);
     } else{
        return META_COMMAND_UNRECOGNIZED_COMMAND;
     }
}

PrepareResult prepare_statement(InputBuffer *input_buffer, Statement *statement){
    if(strncmp(input_buffer->buffer, "insert", 6) == 0){
        statement->type = STATEMENT_INSERT;
        int args_assigned = sscanf(input_buffer->buffer, "Insert %d %s %s", &(statement->row_to_insert.id),
                                    &(statement->row_to_insert.username),
                                &(statement->row_to_insert.email));
        
        if(args_assigned < 3)  return SYNTAX_ERROR;

        return PREPARE_SUCCESS;
    }
    if(strncmp(input_buffer->buffer, "select", 6) == 0){
        statement->type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    return PREPARE_UNRECOGNIZED_STATEMENT;
}

#ifdef _WIN32
ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    if (!*lineptr) { *n = 128; *lineptr = malloc(*n); }
    int c; size_t len = 0;
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= *n) { *n *= 2; *lineptr = realloc(*lineptr, *n); }
        (*lineptr)[len++] = c;
        if (c == '\n') break;
    }
    if (len == 0) return -1;
    (*lineptr)[len] = '\0';
    return len;
}
#endif

void execute_statement(Statement *statement){
    switch(statement->type){
        case(STATEMENT_INSERT):
            printf("This is where we would do an insert.\n");
            break;
        case(STATEMENT_SELECT):
            printf("This is where we would do a select.\n");
            break;
    }
}

void read_input(InputBuffer *input_buffer){
    ssize_t bytes_read =
        getline(&(input_buffer->buffer),
                &(input_buffer->buffer_length), stdin);

    if(bytes_read <= 0){
        printf("Error reading input");
        exit(EXIT_FAILURE);
    }

    // Ignore trailing newline
    input_buffer->buffer_length = bytes_read - 1;
    input_buffer->buffer[bytes_read - 1] = 0;
}

void serialize_row(Row* source, void* destination){
    memcpy((char*)destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy((char*)destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy((char*)destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination){
    memcpy(&(destination->id), (char*)source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), (char*)source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), (char*)source + EMAIL_OFFSET, EMAIL_SIZE);
}

void* row_slot(Table* table, uint32_t row_num){
    uint32_t page_num = row_num / ROWS_PER_PAGE; 
    void* page = table->pages[page_num];
    if(page == NULL){
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return (char*)page + byte_offset;
}

ExecuteResult execute_insert(Statement* statement, Table* table){
    if(table->number_of_rows >= TABLE_MAX_ROWS){
        return EXECUTE_TABLE_FULL;
    }

    Row* row_to_insert = &(statement->row_to_insert);

    serialize_row(row_to_insert, table);
    table->number_of_rows++;

    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement* statement, Table* table){
    if(table->number_of_rows == 0){
        return EXECUTE_TABLE_EMPTY;
    }
    Row row;
    for(uint32_t i = 0; i < table->number_of_rows;i++){
        deserialize_row(row_slot(table, i), &row);
        print_row(&row);
    }
    return EXECUTE_SUCCESS;
}

void print_row(Row* row){
    printf("%d, %s, %s\n", row->id, row->username, row->email);
}

void print_prompt(void){
    printf("db > ");
}