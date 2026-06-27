#include <string.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

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

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = EMAIL_SIZE + USERNAME_SIZE + ID_SIZE;

const uint32_t PAGE_SIZE = 4092;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = TABLE_MAX_PAGES * ROWS_PER_PAGE;

typedef struct{
    u_int32_t number_of_rows;
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

ssize_t getline(char **line, size_t *n, FILE *stream);

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
    memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
    memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
    memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination){
    memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

void* row_slot(Table* table, uint32_t row_num){
    uint32_t page_num = row_num / ROWS_PER_PAGE; 20 / 14
    void* page = table->pages[page_num];
    if(page == NULL){
        page = table->pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return page + byte_offset;
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
    for(u_int32_t i = 0; i < table->number_of_rows;i++){
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