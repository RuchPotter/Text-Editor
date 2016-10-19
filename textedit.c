

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHAR_LIM 81
#define CHAR_LIM_FN 10
#define CHAR_LIM_CMD 5

/* Linked List representation of the text */
typedef struct node{
	char LineString[CHAR_LIM];
	int val;
	struct node *next;
}node;

typedef struct list{
	node *head;
	node *tail;
	int lineNum;
}list;

void read(list *readSpace, char *str){
	node *tmp;
	tmp = (node *)malloc(sizeof(node));
	strcpy(tmp->LineString, str);
	tmp->next = NULL;

	/* operation on appending new nodes */
	if(readSpace->head == NULL){
		/* readSpace list is empty */
		readSpace->head = tmp;
		readSpace->tail = tmp;
	}
	else {
		readSpace->tail->next = tmp;
		readSpace->tail = readSpace->tail->next;
	}
}
void addInt(list *readSpace,int atLine,int myVal){

        node *i;
        int counter;
        counter = 1;

        node *tmp;
        tmp = (node *)malloc(sizeof(node));
        tmp->val=myVal;
        tmp->next = NULL;


        if(atLine==0&&readSpace->head==NULL&&readSpace->tail==NULL){
                tmp->next = readSpace->head;
                readSpace->head = tmp;
                readSpace->tail = tmp;
        }
        else if(atLine==0){

                tmp->next = readSpace->head;
                readSpace->head = tmp;
        }
        else{
                for(i=readSpace->head;i !=NULL; i=i->next){
                        if(counter==atLine){
                                tmp->next = i->next;
                                i->next = tmp;
                        }
                        counter++;
                }
				if(atLine==readSpace->lineNum){
					readSpace->tail=tmp;
				}
        }
        readSpace->lineNum=readSpace->lineNum+1;
}

void freeList(list *l){
        node *temp;
        node *i;
        for(i=l->head;i!=NULL;i=i->next){
                temp = l->head->next;
                free(l->head);
                l->head=temp;
        }
		l->head=NULL;
		l->tail=NULL;
		l->lineNum=0;
}

void print(list *readSpace,char *filen){
	printf("%s\n",filen);
	node *i;
	int count;
	count=0;

	for(i = readSpace->head; i != NULL; i=i->next){
		if(count<9){
			count++;
			printf("0%i %s",count,i->LineString);
		}
		else{
			count++;
			printf("%i %s",count,i->LineString);
		}
	}
}

/* ADD function */
void add(list *readSpace,int atLine,char *restOfString){

	node *i;
	int counter;
	counter = 1;

	node *tmp;
	tmp = (node *)malloc(sizeof(node));
	strcpy(tmp->LineString,restOfString);
	tmp->next = NULL;

	if(atLine==0&&readSpace->head==NULL&&readSpace->tail==NULL){
		tmp->next = readSpace->head;
		readSpace->head = tmp;
		readSpace->tail = tmp;
	}
	else if(atLine==0){
		tmp->next = readSpace->head;
		readSpace->head = tmp;
	}
	else{
		for(i=readSpace->head;i !=NULL; i=i->next){
			if(counter==atLine){
				/* add */
				tmp->next = i->next;
				i->next = tmp;
			}
			counter++;
		}
		if(atLine==readSpace->lineNum){
			/* update tail when adding to last node */
			readSpace->tail=tmp;
		}
	}
	readSpace->lineNum=readSpace->lineNum+1;
}

/* "d" -- delete function */
/* d0 -- delete leaves 0 bytes */
void delete(list *readSpace, int atLine){

	node *i;
	node *prev;
	node *followNode;
	int counter;
	counter = 1;

	if(atLine==0){
		/* delete all */
		freeList(readSpace);
	}
	else{
		if(atLine==1){
			/* delete the first line */
			free(readSpace->head);
			readSpace->head = readSpace->head->next;
		}
		else if(atLine==readSpace->lineNum){
			for(i=readSpace->head; i!=NULL;i=i->next){
				if(counter==readSpace->lineNum-1){
					free(i->next);
					i->next=NULL;
					readSpace->tail = i;
				}
				counter++;
			}
		}
		else{
			prev = readSpace->head;
			for(i=readSpace->head->next; i!=NULL; i=i->next){
				followNode = i->next;
				if(counter==atLine-1){
					free(prev->next);
					prev->next = followNode;
				}
				counter++;
				prev = i;
			}
		}
		readSpace->lineNum--;
	} /* big else */
}

/* "r" -- replace function */
void replace(list *readSpace,int atLine,char *restOfString){

	node *i;
	int counter;
	counter = 1;

	for(i=readSpace->head;i!=NULL;i=i->next){
		if(counter==atLine){
			strcpy(i->LineString,restOfString);
		}
		counter++;
	}
}

/* "s" -- save the modification */
void save(list *readSpace, char *filen){

	FILE * handler;
	handler = fopen(filen,"w");
	if(handler==NULL){
		printf("Can't open file for writing\n");
		fclose(handler);
	}
	else{
		node *i;
		for(i=readSpace->head;i!=NULL;i=i->next){
			fprintf(handler,"%s",i->LineString);
		}
		fclose(handler);
	}
}

void blockInLast(list *readSpace,list *blockBuffer){
        /* Method to insert a block in the last line */
        if(readSpace->head==NULL&&readSpace->tail==NULL){
                readSpace->head = blockBuffer->head;
                readSpace->tail = blockBuffer->tail;
        }
        else{
		readSpace->tail->next = blockBuffer->head;
		readSpace->tail = blockBuffer->tail;
        }
}

void blockInsert(list *readSpace, list *blockBuffer, int atLine){
        /* General block insertion at a line position indicated by atLine */
	node *i;
	int counter;
	counter = 1;

	if(atLine==0){
		
		if(readSpace->head!=NULL&&readSpace->tail!=NULL){
			blockBuffer->tail->next = readSpace->head;
			readSpace->head = blockBuffer->head;
		}
		else{
			blockInLast(readSpace,blockBuffer);
		}
	}
	else{
		for(i=readSpace->head;i!=NULL;i=i->next){
			if(counter==atLine){
				blockBuffer->tail->next = i->next;
				i->next = blockBuffer->head;
			}
			counter++;
		}
	}
}

void popStr(list *myList, char *tmp){
	node *i;
	int cntr;
	cntr=1;
        for(i=myList->head;i!=NULL;i=i->next){
                if(cntr==myList->lineNum){
                        strcpy(tmp,i->LineString);
                }
                cntr++;
        }
        delete(myList,myList->lineNum);
}


int main(){
      /* Work in progress */
	return 0;
}
