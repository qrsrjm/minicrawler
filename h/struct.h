

struct surl {
	int index;
	char rawurl[1024];
 
	char host[256];
	int port;
	char path[1024];
	char post[4096];

	// hlavicky	
	char location[1024];	// presne to co je v hlavicce Location - pro ucely redirect
	char redirectedto[1024];	// co nakonec hlasime ve vystupu v hlavicce
	int chunked;		// 1  pokud transfer-encoding: chunked
	int nextchunkedpos;
	char cookies[20][2][256];	// nekolik cookie, kazda ma name ([0]) a value ([1])
	int cookiecnt;
 
	int state;
	int lastread;		// cas posledniho uspesneho cteni
 
	// ares
	struct ares_channeldata *aresch;
	
	// network
	int sockfd;
	int ip;
	
	// obsah
	char buf[BUFSIZE];
	int bufp;
	int headlen;
	int contentlen;
	int status;		// http navratovy kod
 
};

struct ssettings {
	int debug;
	int timeout;
	int writehead;
	int impatient;
	int partial;
};