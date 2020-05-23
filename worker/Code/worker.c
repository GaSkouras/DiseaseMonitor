#include "../Headers/worker.h"



char * query_handler(char * ,HashTable * , HashTable *, Patient_list*, int );
void open_manual();

Logfile_Info log_info;

int main(int argc, char *argv[])
{
    signal(SIGINT, signal_handler);
    char read_fifo[100];
    char write_fifo[100];
    int buffersize = 10;
    int buffer_counter = 0;
    strcpy(read_fifo, argv[1]);
    strcpy(write_fifo, argv[2]);

    struct stat st;

    int read_fd = open(read_fifo, O_RDONLY);
    // printf("read_fd is %d and pipe is %s\n", read_fd, read_fifo);

    int write_fd = open(write_fifo, O_WRONLY);
    // printf("write fd is %d and pipe is %s \n", write_fd, write_fifo);

    // //printf("read_fifo is %s \n", read_fifo);
    char * message = read_from_fifo(read_fd, buffersize);
    //printf("message from worker.c is %s\n",message );

    Patient_list patient_list;

    
    HashTable disease_HT;
    HashTable country_HT;

    Params params;
    params.disHashSize = 10;
    params.countryHashSize= 5;
    params.bucketsize = 256;

    char *token = strtok(message , "$");
    int prev_token_len = strlen(token);
    
    char *p = message;
    
    while (token)
    {   
        strcpy(params.fileName, token);
        //printf("params filename of process id %u is %s\n", getpid() ,params.fileName );

        //printf("\nProccesing Input...\n");//
        if(strlen(params.fileName)> 10)
        {
            if(readPatientRecordsFile ( params, &disease_HT, &country_HT, &patient_list, write_fd, &log_info)==0)
            {
                printf("\nAn Error Occured While Proccesing Input\n\n");
                exit(0);
            }
        }
        //printf("Parse of file Completed Succesfully!\n\n");

        p = p + (strlen(token) +1);
        token = strtok(p, "\n$");
        if (token)
            prev_token_len += strlen(token);
    }

    free(message);

    // while(1)
    // {
    //     //printf("beno sto aenao loop\n");
    //     message = read_from_fifo(read_fd, buffersize);
    //     printf("message is %s\n", message);
    //     message = query_handler(message, &disease_HT, &country_HT, &patient_list, write_fd);
    //     //diaxeirizomai to message otan erthei kai etoimazo tin apantisi
    //     //write_to_fifo (write_fd, result); //to message tha prokipsei einai to apotelesma tou query
    // }
    
    //cli(&disease_HT, &country_HT, &patient_list);
    destroyHashTable(&disease_HT);
    destroyHashTable(&country_HT);
    freePatientList(&patient_list);

}


void signal_handler()
{
    char buffer[30] = " ";
    // sprintf(buffer, "%u", getpid());
    sprintf(buffer, "log_file.");
    sprintf(buffer + strlen(buffer), "%u", getpid());

    FILE *fp = fopen(buffer, "w");

    //printf("buffer in signal is %s\n",buffer );
    fprintf(fp, "Total: %d\n", log_info.total);
    fprintf(fp, "Success: %d\n", log_info.success);
    fprintf(fp, "Fail: %d\n" , log_info.fail);

    fclose(fp);
}

char * read_from_fifo( int read_fd, int buffersize)
{
    int bytes_in; //posa byte diavastikan apo tin read
    int buffer_counter = 0;


    char temp[11];

    read(read_fd, temp, 10);

    char *token = strtok(temp, "$");
    int input_size = atoi(token); //tora ksero posa byte tha mou steilei

    char * buffer = malloc(sizeof(char) * (input_size+1));

    char *p = buffer;

    //printf("char *p is %s\n", p);

    while(buffer_counter < input_size)
    {
        bytes_in = read( read_fd,p, buffersize );//printf("bytes_in = %d\n", bytes_in);
        
        if(buffer_counter + bytes_in > input_size)
            bytes_in = input_size-buffer_counter;
        
        p += bytes_in;
        buffer_counter += bytes_in;
    }

    buffer[input_size]='\0';
   //printf("buffer to return is %s\n", buffer);
    return buffer;
}

void write_to_fifo(int  write_fd, char * message)
{
    int message_len = strlen(message);
    char temp[11];

    sprintf(temp, "%d$", message_len);
    //printf("message_len is %d\n", message_len);
    write(write_fd, temp, 10);
    //printf("message is %s\n",message );
    write(write_fd, message, message_len);
}


void write_summary_stats( HashTable * disease_HT,  char * country, Date date, int write_fd)
{
    char stats[1000];
    // print_date(date);
    // printf("\n");
    // printf("%s\n",country);
    sprintf(stats, "%d-%d-%d", date.day, date.month, date.year);
    sprintf(stats, "%s\n%s", stats, country);

    //printf("%s\n", stats);

    for (int i = 0; i < disease_HT->size; ++i)
    {       
        if( disease_HT->lists_of_buckets[i].head != NULL)
        {
            Bucket_Node *temp = disease_HT->lists_of_buckets[i].head;
            while(temp !=NULL)
            {                    
                for (int j = 0; j < temp->slot_counter; ++j)
                {   
                    // printf("%s\n", temp->bucket_item[j].string);
                    sprintf(stats, "%s\n%s", stats, temp->bucket_item[j].string);
                    sprintf(stats, "%s\nAge range 0-20 years: %d", stats, temp->bucket_item[j].age_ranges[0]);
                    sprintf(stats, "%s\nAge range 20-40 years: %d", stats, temp->bucket_item[j].age_ranges[1]);
                    sprintf(stats, "%s\nAge range 41-60 years: %d", stats, temp->bucket_item[j].age_ranges[2]);
                    sprintf(stats, "%s\nAge range 60+ years: %d", stats, temp->bucket_item[j].age_ranges[3]);
                }
                temp = temp->next;
            }
        }
    }
    write_to_fifo(write_fd, stats);
}

int readPatientRecordsFile ( Params params, HashTable * disease_HT, HashTable * country_HT, Patient_list *patient_list, int write_fd, Logfile_Info *log_info)
{
    //printf("file name is %s\n",params.fileName );

    struct dirent *de;
    // if(strcmp(params.fileName, "./resources/input_dir/China")==0)
    //     printf("profanos kai einai isa\n");

    DIR *dr = opendir(params.fileName);

    //printf("To directory einai %s\n",params.fileName);

    if (dr == NULL)  // opendir returns NULL if couldn't open directory 
    { 
        perror("Error dr == NULL ");
        exit(0);
    } 

    char *line = NULL;
    size_t len = 0;
    ssize_t nread;
    int line_pos;
    char file_path[200] = " ";
    char date[50];

    strcpy(file_path, params.fileName);

    Patient patient_attributes; 

    Patient_Node * new_patient_node = NULL;

    log_info->total = 0;
    log_info->success = 0;
    log_info->fail = 0;

    initHashTable(disease_HT, params.disHashSize, params.bucketsize); 
    initHashTable(country_HT, params.countryHashSize, params.bucketsize); 

    initPatientList(patient_list);

    while ((de = readdir(dr)) != NULL)
    {
        if(strcmp(de->d_name, ".") && strcmp(de->d_name, ".."))
        {
            //printf("%s\n",de->d_name );
            strcat(file_path, "/");
            strcat(file_path, de->d_name);
    
            //printf("file path is %s\n",file_path );
            FILE *fp = fopen(file_path, "r");

            if (fp == NULL) //error handling when opening the file
            {
                perror(" Requested file failed to open");
                return 0;
            }

            while ((nread = getline(&line, &len, fp)) != -1) 
            {   
                strcpy(date, de->d_name);
                char temp_line[100] = " ";

                for (int i = 0; line[i] != '\0'; ++i) 
                {
                    temp_line[i] = line[i];
                }

                //printf("temp_lineis %s\n", temp_line);

                patient_attributes = line_tokenize(line, patient_attributes, date, file_path);
                
                if (id_exist(patient_list, patient_attributes.recordID))
                {
                    printf("Patient with Record ID %s has been already inserted, thus it ommited\n", patient_attributes.recordID);
                    free(patient_attributes.recordID);
                    free(patient_attributes.firstName);
                    free(patient_attributes.lastName);
                    free(patient_attributes.diseaseID);
                    free(patient_attributes.country);
                    log_info->fail++;
           
                }
                else if(strcmp(patient_attributes.status, "EXIT") == 0 && !id_exist(patient_list, patient_attributes.recordID))
                {
                    //printf("Provlimatiki eggrafi me stoixeia");
                    //printPatientData(patient_attributes);
                    free(patient_attributes.recordID);
                    free(patient_attributes.firstName);
                    free(patient_attributes.lastName);
                    free(patient_attributes.diseaseID);
                    free(patient_attributes.country);
                    log_info->fail++;
           
                }
                else if(!id_exist(patient_list, patient_attributes.recordID))
                {                
                    new_patient_node =  insertNewPatient(patient_list, patient_attributes);
                    insert_to_hash_table(disease_HT, patient_attributes.diseaseID, new_patient_node);
                    //insert_to_hash_table(country_HT, patient_attributes.country, new_patient_node);
                    log_info->success++;
                }  

                log_info->total ++;
            }
            write_summary_stats(disease_HT, patient_list->tail->patient.country, patient_list->tail->patient.entryDate, write_fd);
            fclose(fp);
            strcpy(file_path, params.fileName);
           //printf("file_path is %s\n", file_path);
        }
    }
    // // print_hash_table(disease_HT);line 

    free(line);
    closedir(dr);   
    return 1;
}


int digitValidate(char *a)
{
	for (unsigned int i = 0; i < strlen(a); i++)
	{
		if(!isdigit(a[i]))
			return -1;
	}
	return 0;
}


Patient line_tokenize(char *line, Patient patient, char * date, char * country )
{
        char * token;
        char temp [50] = " ";
        char temp_line[50]= " ";
        strcpy(temp, country);
        strcpy(temp_line, line );

        //printf("line is %s\n", temp_line );
        token = strtok(temp_line, " ");
        patient.recordID = malloc(sizeof(char)* strlen(token)+1);
        strcpy(patient.recordID, token);
        //printf("patient record id is %s\n", patient.recordID);

        token = strtok(NULL, " ");
        strcpy( patient.status, token);
        //printf("patient record status is %s\n", patient.status);

        token = strtok(NULL, " ");
        patient.firstName = malloc(sizeof(char)*strlen(token)+1);
        strcpy( patient.firstName, token);
        //printf("patient record firstName is %s\n", patient.firstName);

        token = strtok(NULL, " ");
        patient.lastName = malloc(sizeof(char)*strlen(token)+1);
        strcpy( patient.lastName, token);
        //printf("patient record lastName is %s\n", patient.lastName);

        token = strtok(NULL, " ");
        patient.diseaseID = malloc(sizeof(char)*strlen(token)+1);
        strcpy( patient.diseaseID, token);
        //printf("patient record disease is %s\n", patient.diseaseID);

        token = strtok(NULL, " ");
        patient.age = atoi(token);
        //printf("patient record age is %d\n", patient.age);

        if(strcmp(patient.status, "ENTER") == 0)
        {
            token = strtok(date, "-");
            patient.entryDate.day = atoi (token);

            token = strtok(NULL, "-");
            patient.entryDate.month = atoi (token);

            token = strtok(NULL, "\n");
            patient.entryDate.year = atoi (token);
            
            patient.exitDate.day = 0;
            patient.exitDate.month = 0;
            patient.exitDate.year = 0;

        }
        else
        {
            token = strtok(date, "-");
            patient.exitDate.day = atoi (token);

            token = strtok(NULL, "-");
            patient.exitDate.month = atoi (token);

            token = strtok(NULL, "\n");
            patient.exitDate.year = atoi (token);

            patient.entryDate.day = 0;
            patient.entryDate.month = 0;
            patient.entryDate.year = 0;

        } 
        //printf("country is %s\n",country );
        token = strtok(temp, "/");
        token = strtok(NULL, "/");
        token = strtok(NULL, "/");
        token = strtok(NULL, "/");

        patient.country = malloc(sizeof(char)*strlen(token)+1);
        strcpy( patient.country, token); 

    return patient;
}
