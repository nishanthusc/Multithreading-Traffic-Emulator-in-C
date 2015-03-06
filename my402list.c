#include<stdio.h>
#include<stdlib.h>
#include "cs402.h"
#include "my402list.h"

int My402ListInit(My402List* list){

	list->anchor.prev = &(list->anchor);
	list->anchor.next = &(list->anchor);
	list->anchor.obj = NULL;
	list->num_members = 0;	
	return TRUE;

}

int  My402ListEmpty(My402List* list){

	if((list->anchor.prev== &(list->anchor))&&(list->anchor.next == &(list->anchor))){
		return TRUE;
	}
	else{
		return FALSE;		
	}

}

int  My402ListAppend(My402List* list, void* dat){


	My402ListElem* newNode = (My402ListElem*)malloc(sizeof(My402ListElem)); // creaating a new node
	if(!newNode){
		printf("Error in allocating memory to newNode");
		return FALSE;
	}
	My402ListElem* lastElem;
	if(My402ListEmpty(list)){
		//when list is empty adding a new element to the list and updating the pointers
		list->anchor.next = newNode;
		list->anchor.prev = newNode;
		newNode->prev = &(list->anchor);
		newNode->next = &(list->anchor);
		newNode->obj = dat;
		list->num_members++;
		return TRUE;
	}
	else{
		//not verifying if the lastElem is null because we have already checked with Empty function
		// if the list is empty or not
		lastElem = My402ListLast(list);
		lastElem->next = newNode;
		newNode->prev = lastElem;
		newNode->next = &(list->anchor);
		list->anchor.prev = newNode;
		newNode->obj = dat;
		list->num_members++;
		return TRUE;
	}

} // End of the MyList Append Function



int  My402ListPrepend(My402List* list, void* dat){


	My402ListElem* newNode = (My402ListElem*)malloc(sizeof(My402ListElem)); // creaating a new node
	if(!newNode){
		printf("Error in allocating memory to newNode");
		return FALSE;
	}
	My402ListElem* firstElem;
	if(My402ListEmpty(list)){
		//when list is empty adding a new element to the list and updating the pointers
		list->anchor.next = newNode;
		list->anchor.prev = newNode;
		newNode->prev = &(list->anchor);
		newNode->next = &(list->anchor);
		newNode->obj = dat;
		list->num_members++;
		return TRUE;
	}
	else{
		//not verifying if the lastElem is null because we have already checked with Empty function
		// if the list is empty or not
		firstElem = My402ListFirst(list);
		list->anchor.next = newNode;
		newNode->prev = &(list->anchor);
		newNode->next = firstElem;
		firstElem->prev = newNode;
		newNode->obj = dat;
		list->num_members++;
		return TRUE;
	}

} // End of the MyList Prepend Function

void My402ListUnlink(My402List* list, My402ListElem* elem){

	My402ListElem *temp,*temp2;
	
	//checking if the element is the only element on the list		
	/*if((elem->prev == &(list->anchor)) && (elem->next == &(list->anchor))){
	
		list->anchor.prev = &(list->anchor);
		list->anchor.next = &(list->anchor);
		
		free(elem);
		list->num_members--;
	} 
	else{*/
		
		temp = elem->prev;//My402ListPrev(list,elem) ;
//		temp->next = My402ListNext(list,elem);
		temp2 = elem->next; //My402ListNext(list,elem);
		temp2->prev = elem->prev;
		temp->next = elem->next;
		free(elem);
		list->num_members--;
	/*}*/

}


void My402ListUnlinkAll(My402List* list){

	if(list->anchor.next!=&(list->anchor)){
		My402ListElem* ele = list->anchor.next;
		My402ListElem* temp;
		while(ele->next!=&(list->anchor)){

			temp = ele->next;
			free(ele);
			ele = temp;
			list->num_members--;

		}

	}
}


int  My402ListInsertAfter(My402List* list, void* dat, My402ListElem* elem){
	
	
	if(elem){
		//checking if the elem is null; if it is  null then it is same as prepend
		My402ListElem* newN = (My402ListElem*) malloc(sizeof(My402ListElem));
		My402ListElem* temp;
		if(!newN){

			printf("Error in creating memory");
			return FALSE;
		}
		newN->next = elem->next;
		newN->prev = elem;
		temp = elem->next;
		//(elem->next)->prev = newN;
		temp->prev = newN;
		elem->next = newN;
		newN->obj = dat;
		list->num_members++;
		return TRUE;
		
	}
	else{

		int status = My402ListPrepend(list,dat);
		list->num_members++;
		return status;

	}

}
int  My402ListInsertBefore(My402List* list, void* dat, My402ListElem* elem){
	
	
	if(elem){
		//checking if the elem is null; if it is  null then it is same as prepend
		My402ListElem* newN = (My402ListElem*) malloc(sizeof(My402ListElem));
		My402ListElem* temp;
		if(!newN){

			printf("Error in creating memory");
			return FALSE;
		}
		newN->next = elem;
		newN->prev = elem->prev;
		temp = elem->prev;
		//(elem->prev)->next = newN;
		temp->next = newN;
		elem->prev = newN;
		newN->obj = dat;
		list->num_members++;
		return TRUE;
		
	}
	else{

		int status = My402ListAppend(list,dat);
		list->num_members++;
		return status;

	}

}



My402ListElem *My402ListFirst(My402List* list){

	if(list->anchor.prev==&(list->anchor)){
		return NULL;
	}	
	else{
		return (list->anchor.next);

	}
}

My402ListElem *My402ListLast(My402List* list){

	if(list->anchor.prev==&(list->anchor)){
		return NULL;
	}	
	else{
		return (list->anchor.prev);

	}
}

My402ListElem *My402ListNext(My402List* list, My402ListElem* elem){

	if(list->anchor.prev==elem){

		return NULL;
	}
	
	else{
		return (elem->next);		

	}

}


My402ListElem *My402ListPrev(My402List* list, My402ListElem* elem){

	if(list->anchor.next == elem){
		return NULL;
	}
	else{
		return(elem->prev);
	}

}

int  My402ListLength(My402List *list){
        /*My402ListElem *elem=NULL;

        for (elem=My402ListFirst(list);elem != NULL;elem=My402ListNext(list, elem)) {
            list->num_members = list->num_members+1;

            
        }*/

	return (list->num_members);
 }

My402ListElem *My402ListFind(My402List* list, void* dat){

	My402ListElem *elem=NULL;

        for (elem=My402ListFirst(list);elem != NULL;elem=My402ListNext(list, elem)) {
            if(elem->obj == dat){
		return (elem);

	     }
	         
        }

	return NULL;  

}






