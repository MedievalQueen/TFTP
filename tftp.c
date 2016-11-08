/* A tftp client implementation.
   Author: Erik Nordström <eriknord@it.uu.se>
*/
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/select.h>

#include "tftp.h"

extern int h_errno;

#define TFTP_TYPE_GET 0
#define TFTP_TYPE_PUT 1

/* Should cover most needs */
#define MSGBUF_SIZE (TFTP_DATA_HDR_LEN + BLOCK_SIZE)


/*
 * NOTE:
 * In tftp.h you will find definitions for headers and constants. Make
 * sure these are used throughout your code.
 */


/* A connection handle */
struct tftp_conn {
	int type; /* Are we putting or getting? */
	FILE *fp; /* The file we are reading or writing */
	int sock; /* Socket to communicate with server */
	int blocknr; /* The current block number */
	char *fname; /* The file name of the file we are putting or getting */
	char *mode; /* TFTP mode */
	struct sockaddr_in peer_addr; /* Remote peer address */
	socklen_t addrlen; /* The remote address length */
	char msgbuf[MSGBUF_SIZE]; /* Buffer for messages being sent or received */
};

/* Close the connection handle, i.e., delete our local state. */
void tftp_close(struct tftp_conn *tc)
{
	if (!tc)
		return;

	fclose(tc->fp);
	close(tc->sock);
	free(tc);
}

/* Connect to a remote TFTP server.    type=get or put mode=octet*/
struct tftp_conn *tftp_connect(int type, char *fname, char *mode,
			       const char *hostname)
{
    struct addrinfo hints;
    struct addrinfo * res = NULL;
	struct tftp_conn *tc;

	if (!fname || !mode || !hostname)
		return NULL;

	tc = malloc(sizeof(struct tftp_conn));

	if (!tc)
		return NULL;

	/* Create a socket.
	 * Check return value. */
	tc->sock= socket(AF_INET, SOCK_DGRAM, 0);
	if(tc->sock < 0){
		fprintf(stderr, "Cant create socket.\n");
		return NULL;
	}

	/* ... */

	if (type == TFTP_TYPE_PUT)
		tc->fp = fopen(fname, "rb");
	else if (type == TFTP_TYPE_GET)
		tc->fp = fopen(fname, "wb");
	else {
		fprintf(stderr, "Invalid TFTP mode, must be put or get.\n");
		return NULL;
	}

	if (tc->fp == NULL) {
		fprintf(stderr, "File I/O error.\n");
		close(tc->sock);
		free(tc);
		return NULL;
	}

    memset(&hints,0,sizeof(hints));
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    char port_str[5];
    sprintf(port_str, "%d", TFTP_PORT);//TFTP_PORT is 69

    /* get address from host name.
	 * If error, gracefully clean up.*/
    int status;
    if((status=getaddrinfo(hostname, port_str, &hints, &res)) !=0 ){
    	fprintf(stderr, "getaddressinfo error: %s\n", gai_strerror(status));
    	//free(res);
    	exit(1);
    }
	/* ... */

	/* Assign address to the connection handle.
	 * You can assume that the first address in the hostent
	 * struct is the correct one */

	memcpy(&tc->peer_addr, res->ai_addr, res->ai_addrlen);

	tc->addrlen = sizeof(struct sockaddr_in);

	tc->type = type;
	tc->mode = mode;
	tc->fname = fname;
	tc->blocknr = 0;

	memset(tc->msgbuf, 0, MSGBUF_SIZE);

	//connect(tc->sock, res->ai_addr, res->ai_addrlen);

	return tc;
}

/*
  Send a read request to the server
  1. Format message
  2. Send the request using the connection handle
  3. Return the number of bytes sent, or negative on error
 */
int tftp_send_rrq(struct tftp_conn *tc)
{
	/* struct tftp_rrq *rrq; */

        /* ... */
		/*
			struct tftp_rrq {
				u_int16_t opcode;
				char req[0];
			};
		*/
		struct tftp_rrq *rrq;
		rrq=NULL;
		rrq->opcode=htons(OPCODE_RRQ);// htons transforms from host to network byte order
		strcpy(&rrq->req[0], tc->fname);
		strcpy(&rrq->req[strlen(tc->fname)+1], tc->mode);
		int len= TFTP_RRQ_LEN(tc->fname, tc->mode);
		memcpy(tc->msgbuf,rrq, len);
		int b = sendto(tc->sock, rrq, len, 0, (struct sockaddr *)& tc->peer_addr, tc->addrlen);

        return b;
}
/*

  Send a write request to the server.
  1. Format message.
  2. Send the request using the connection handle.
  3. Return the number of bytes sent, or negative on error.
 */
int tftp_send_wrq(struct tftp_conn *tc)
{	// opcode(2 bytes), filename(), 1 byte, mode(), 1byte
	/* struct tftp_wrq *wrq; */

        /* ... */
	
	/*struct tftp_wrq {
	u_int16_t opcode;
	char req[0];
	};*/	

	struct tftp_wrq *wrq;
	wrq->opcode=htons(OPCODE_WRQ);// htons transforms from host to network byte order
	strcpy(&wrq->req[0], tc->fname);
	strcpy(&wrq->req[strlen(tc->fname)+1], tc->mode);
	int len= TFTP_WRQ_LEN(tc->fname, tc->mode);
	memcpy(tc->msgbuf,wrq, len);
	int b = sendto(tc->sock, wrq, len, 0, (struct sockaddr*)&tc->peer_addr, tc->addrlen);

    return b;
}


/*
  Acknowledge reception of a block.
  1. Format message.
  2. Send the acknowledgement using the connection handle.
  3. Return the number of bytes sent, or negative on error.
 */
int tftp_send_ack(struct tftp_conn *tc)
{
	/* struct tftp_ack *ack; */
		struct tftp_ack *ack;
		ack->opcode=htons(OPCODE_ACK);
		//tc->blocknr=ntohs(((struct tftp_data*) tc->msgbuf)->blocknr);
		//ack->blocknr=htons(tc->blocknr);
        tc->blocknr=ntohs(((struct tftp_data*) tc->msgbuf)->blocknr);
        ack->blocknr=htons(tc->blocknr);
        int b = sendto(tc->sock, ack, TFTP_ACK_HDR_LEN, 0, (struct sockaddr*)&tc->peer_addr, tc->addrlen);

        return b;
}

/*
  Send a data block to the other side.
  1. Format message.
  2. Add data block to message according to length argument.
  3. Send the data block message using the connection handle.
  4. Return the number of bytes sent, or negative on error.

  TIP: You need to be able to resend data in case of a timeout. When
  resending, the old message should be sent again and therefore no
  new message should be created. This can, for example, be handled by
  passing a negative length indicating that the creation of a new
  message should be skipped.
 */
int tftp_send_data(struct tftp_conn *tc, int length)
{
	/* struct tftp_data *tdata; */
	struct tftp_data *tdata;
	tdata->opcode=htons(OPCODE_DATA);
	tc->blocknr=ntohs(((struct tftp_ack*)tc->msgbuf)->blocknr) + 1;
	tdata->blocknr=htons(tc->blocknr);
	if(!feof(tc->fp)){
		int dl=fread(tdata->data,1, length, tc->fp); // reads 1*length bytes of tc->fp into data
		int b = sendto(tc->sock, tdata, dl+TFTP_DATA_HDR_LEN, 0, (struct sockaddr *) &tc->peer_addr, tc->addrlen);
		return b;
	}
    return -1;
}

/*
  Transfer a file to or from the server.

 */
int tftp_transfer(struct tftp_conn *tc)
{
	int retval = 0;
	int len;
	int totlen = 0;
	struct timeval timeout;
	int end=0;
        /* Sanity check */
	u_int16_t err;
	u_int16_t msgg;

	if (!tc)
		return -1;

        len = 0;

	/* After the connection request we should start receiving data
	 * immediately */

        /* Set a timeout for resending data. */

        timeout.tv_sec = TFTP_TIMEOUT;
        timeout.tv_usec = 0;

        /* Check if we are putting a file or getting a file and send
         * the corresponding request. */
       	
      	/* ... */
       	if (tc->type == TFTP_TYPE_PUT){
       		if(tftp_send_wrq(tc)<0)//if returns -1 is error
 			{
 				fprintf(stderr, "send wrq error");
 				exit(1);
 			}      			
       	}
		else if (tc->type == TFTP_TYPE_GET){
			if(tftp_send_rrq(tc)<0)
			{
 				fprintf(stderr, "send rrq error");
 				exit(1);
 			}
		}else{
			//goto out;
		}

        /*
          Put or get the file, block by block, in a loop.
         */
   	fd_set setfd;
	do {
		/* 1. Wait for something from the server (using
                 * 'select'). If a timeout occurs, resend last block
                 * or ack depending on whether we are in put or get
                 * mode. */

                /* ... */
                 FD_ZERO(&setfd);
                 FD_SET(tc->sock, &setfd);
               	 int i= select(tc->sock +1, &setfd, NULL, NULL, &timeout);
        switch(i){
        	case -1:
        		fprintf(stderr, "send rrq error");
        		retval = -1;
 				//goto out;
 				break;
 			case 0:	//timeout
 				retval=0;
 				if(tc->type==TFTP_TYPE_GET){
 					if(tc->blocknr==0)  //if sent 1st read req, send again
 						tftp_send_rrq(tc);
 					else{ //send previous ACK again
 						//tc->blocknr--;
 						tftp_send_ack(tc);					
 					}
 				}else if(tc->type==TFTP_TYPE_PUT){
 					if(tc->blocknr==0)
 						tftp_send_wrq(tc);
 					else{ //retransmit data, cus didnt receive ACK
 						len=tftp_send_data(tc, BLOCK_SIZE); //BLOCK_SIZE=512
 					}
 				}else{
 					//goto out;
 				}
 				break;
 			default:
 				len=recvfrom(tc->sock, tc->msgbuf, sizeof(tc->msgbuf), 0, (struct sockaddr *) &tc->peer_addr, &tc->addrlen);
 				//if((len==0) || (len==-1))
 					//goto out;
 				break;
        }
        msgg=ntohs(((struct tftp_data*) tc->msgbuf)->opcode);

                /* 2. Check the message type and take the necessary
                 * action. */
		switch ( msgg/* change for msg type */ ) {
		case OPCODE_DATA:
                        /* Received data block, send ack */
			tc->blocknr=ntohs(((struct tftp_data*) tc->msgbuf)->blocknr);
			fwrite(tc->msgbuf+TFTP_DATA_HDR_LEN, 1, len- TFTP_DATA_HDR_LEN, tc->fp);
			totlen+=len - TFTP_DATA_HDR_LEN;
			tftp_send_ack(tc);
			if(len<(BLOCK_SIZE+4)){
				end=1;
			}
			break;

		case OPCODE_ACK:
                        /* Received ACK, send next block */
			tc->blocknr=ntohs(((struct tftp_ack*) tc->msgbuf)->blocknr);
			totlen+=len - TFTP_DATA_HDR_LEN;
			if(len>0 && len<(BLOCK_SIZE+4)){
				end=1;
				break;
			}
			len=tftp_send_data(tc, BLOCK_SIZE);
		 	break;

		case OPCODE_ERR:
                        /* Handle error... */
			err=ntohs(((struct tftp_err*) tc->msgbuf)->errcode);
			fprintf(stderr, "%s\n", err_codes[err]);
			retval = -1;
			//goto out;

		default:
			fprintf(stderr, "\nUnknown message type\n");
			//goto out;
		}

	} while ( end==0 /* 3. Loop until file is finished */);

	printf("\nTotal data bytes sent/received: %d.\n", totlen);
	//out:  
		return retval;
}

int main (int argc, char **argv)
{

	char *fname = NULL;
	char *hostname = NULL;
	char *progname = argv[0];
	int retval = -1;
	int type = -1;
	struct tftp_conn *tc;

        /* Check whether the user wants to put or get a file. */
	while (argc > 0) {

		if (strcmp("-g", argv[0]) == 0) {
			fname = argv[1];
			hostname = argv[2];

			type = TFTP_TYPE_GET;
			break;
		} else if (strcmp("-p", argv[0]) == 0) {
			fname = argv[1];
			hostname = argv[2];

			type = TFTP_TYPE_PUT;
			break;
		}
		argc--;
		argv++;
	}

        /* Print usage message */
	if (!fname || !hostname) {
		fprintf(stderr, "Usage: %s [-g|-p] FILE HOST\n",
			progname);
		return -1;
	}

        /* Connect to the remote server */
	tc = tftp_connect(type, fname, MODE_OCTET, hostname);

	if (!tc) {
		fprintf(stderr, "Failed to connect.\n");
		return -1;
	}

        /* Transfer the file to or from the server */
	retval = tftp_transfer(tc);

        if (retval < 0) {
                fprintf(stderr, "File transfer failed.\n");
        }

        /* We are done. Cleanup our state. */
	tftp_close(tc);

	return retval;
}
