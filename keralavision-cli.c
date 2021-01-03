/* Anoop S  */
#include <stdio.h>
#include <string.h>
#include <curl/curl.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <termios.h>

#define VERSION "1.0"
#define MAX_LINE 4096
#define MAX_CREDS 128


#define log_err(MESSAGE, ...) fprintf(stderr,\
        "[ERROR] " MESSAGE "\n", ##__VA_ARGS__)
	
#define check_fail(FAIL_CONDITION, MESSAGE, ...) if((FAIL_CONDITION)) {\
    log_err(MESSAGE, ##__VA_ARGS__); exit(EXIT_FAILURE); }


struct App{
	const char *base_url;
	const char *login_url;
	const char *gauge_url;
	char EVENTVALIDATION[MAX_LINE];
	char VIEWSTATE[MAX_LINE];
	char UserName [MAX_CREDS];
	char Password [MAX_CREDS];
};

struct webpage {
  char *str;
  size_t len;
};


struct Gauge{
	const char *total_usage_id;
	const char *session_id;
	const char *plan_id;
	const char *expiry_id;
	const char *mac_id;
	const char *ip_id;
};

/* fgets for string , get string line by  line,cause strstr smashes the stack on large data */
char *sgets( char * str, int num, char **input )
{
    char *next = *input;
    int  numread = 0;

    while ( numread + 1 < num && *next ) {
        int isnewline = ( *next == '\n' );
        *str++ = *next++;
        numread++;
        if ( isnewline )
            break;
    }

    if ( numread == 0 )
        return NULL; 

    *str = '\0';  
    *input = next;
    return str;
}

int get_password(struct App *app, size_t size){
	struct termios oflags, nflags;

	tcgetattr(fileno(stdin), &oflags);
    nflags = oflags;
    nflags.c_lflag &= ~ECHO;
    nflags.c_lflag |= ECHONL;

	if (tcsetattr(fileno(stdin), TCSADRAIN, &nflags) != 0) {
        perror("tcsetattr");
        return -1;
    }
    printf("Enter KV Password : ");
	/* to avoid new line terminating fgets */
	scanf("\n");
    fgets(app->Password, size, stdin);
	app->Password[strlen(app->Password)-1] = '\0';


    if (tcsetattr(fileno(stdin), TCSANOW, &oflags) != 0) {
        perror("tcsetattr");
        return -1;
    }
	return 0;
}




void init_webpage(struct webpage *page) {
  page->len = 0;
  page->str = malloc(page->len+1);
  check_fail(page->str == NULL, "malloc failed");
  page->str[0] = '\0';
}

size_t writefunc(void *ptr, size_t size, size_t nmemb, struct webpage *page)
{
  size_t new_len = page->len + size*nmemb;
  page->str = realloc(page->str, new_len+1);
  check_fail(page->str == NULL, "realloc failed");
  memcpy(page->str+page->len, ptr, size*nmemb);
  page->str[new_len] = '\0';
  page->len = new_len;

  return size*nmemb;
}

/* Hack to silence curl  */
size_t write_null(void *buffer, size_t size, size_t nmemb, void *userp)
{
   return size * nmemb;
}


/* Get the Log in web page to extract data for post request */
int get_webpage(const char *url, struct webpage *page){

  CURL *curl;
  CURLcode res;

  curl = curl_easy_init();
  if(curl) {
    init_webpage(page);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, page);
    res = curl_easy_perform(curl);
	check_fail(res != CURLE_OK, "curl_easy_perform() failed: %s",curl_easy_strerror(res));
  }
    /* always cleanup */
    curl_easy_cleanup(curl);
  	return 0;

}


/* extract the data from log in web page */
int extract_token(const char *token_name, char *page_string, char *token_dest){
	char line[MAX_LINE];

	 while ( sgets( line, MAX_LINE-1, &page_string )){
		char *result = strstr(line,token_name);

		if(result != NULL){
					int pos;
					int substring_length;
					const char *value_prefix =  " value=\"";
					const char *trailing_str = " \" />.";
					pos = (result - line) + strlen(token_name) + strlen(value_prefix);
					substring_length = strlen(line) - pos;
					strncpy(token_dest,line+ pos,substring_length - strlen(trailing_str));
					break;
				}
		}
	 check_fail(token_dest == NULL,"failed to extract %s",token_name);
	 return 0;

}

/* extract desired user data from KV User information page */
char* extract_gauge(const char *gauge_data_to_extract,char *page_string, char *gauge_data_dest){
	char line[MAX_LINE];
	char *found;

	while(sgets(line , MAX_LINE-1, &page_string)){
		found = strstr(line,gauge_data_to_extract);
		if(found != NULL){
			int pos;
			int substring_length;
			char *start = strstr(line,">") + 1;

			pos = (start - line) ;
			char *end = strstr(line,"</span>") - 1;
			substring_length = (end - start) + 1;

			strncpy(gauge_data_dest,line + pos, substring_length);
			/* Null terminate the string */
			gauge_data_dest[substring_length] = '\0';
			break;

		}
	}

	check_fail(found == NULL,"Cannot extract %s\nProbably wrong Password",gauge_data_to_extract);
	return gauge_data_dest;
}


int get_all_tokens(struct webpage *page, struct App *app){

	extract_token("id=\"__VIEWSTATE\"", page->str, app->VIEWSTATE);
	extract_token("id=\"__EVENTVALIDATION\"" , page->str, app->EVENTVALIDATION);
	return 0;
}

/* get the actual page which contains user data */
int get_gauge_page(struct App *app,struct webpage *page){

	char post_params[10240];

	CURL *curl;
	CURLcode res;

	curl_global_init(CURL_GLOBAL_ALL);
	curl = curl_easy_init();

	if(curl) {
		char user_creds[512];
		/* Encode post params */
		char *view_encoded = curl_easy_escape(curl, app->VIEWSTATE, strlen(app->VIEWSTATE));
		strcpy(post_params,"__VIEWSTATE=");
		strcat(post_params,view_encoded);
		curl_free(view_encoded);

		char *event_encoded = curl_easy_escape(curl, app->EVENTVALIDATION, strlen(app->EVENTVALIDATION));
		strcat(post_params,"&__EVENTVALIDATION=");
		strcat(post_params,event_encoded);
		curl_free(event_encoded);

		snprintf(user_creds,511,"&txtUserName=%s&txtPassword=%s&hdnloginwith=username&save=Log+In&txtForgetCapcha=",app->UserName,app->Password);
		strcat(post_params,user_creds);


		init_webpage(page);
		/* Login */
		curl_easy_setopt(curl, CURLOPT_URL, app->login_url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_null);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_params);
		curl_easy_setopt(curl, CURLOPT_COOKIEFILE, ""); /* start cookie engine */ 
		res = curl_easy_perform(curl);
		check_fail(res != CURLE_OK, "curl_easy_perform() failed: %s",curl_easy_strerror(res));
		/* Get gauge webpage */
		curl_easy_setopt(curl, CURLOPT_URL, app->gauge_url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writefunc);
    	curl_easy_setopt(curl, CURLOPT_WRITEDATA, page);
		res = curl_easy_perform(curl);
		check_fail(res != CURLE_OK, "curl_easy_perform() failed: %s",curl_easy_strerror(res));
    	curl_easy_cleanup(curl);
  }
  curl_global_cleanup();
  return 0;
}

void print_all_gauge_data(struct Gauge *gauge, char *page_str){
	char gauge_dest[64];
	char total_data[64];
	char total_used[64];
	char remaining_data_str[64];
	char plan_name[64];
	extract_gauge(gauge->total_usage_id, page_str, total_used);
	extract_gauge(gauge->plan_id ,page_str,plan_name);

	if(strstr(plan_name,"UL") != NULL){
		strcpy(remaining_data_str,"Unlimited");
	}
	else{
		int i;
		char *found = strchr(plan_name, 'M');
		if(found != NULL){
			for(found++, i=0; isdigit(*found); found++, i++){
				total_data[i] = *found;
			}
			total_data[i] = '\0';
		}
		
		unsigned int remaining_data = atoi(total_data) - atoi(total_used);
		sprintf(remaining_data_str,"%d GB",remaining_data);
	}

	printf("===================================\n");
	printf("\t   %s\n",plan_name);
	printf("===================================\n");
	printf("* Total Used Data : %s / %s GB\n", total_used, total_data);
	printf("\n* Remaining Data : %s\n",remaining_data_str);
	printf("\n* Session : %s\n", (extract_gauge(gauge->session_id ,page_str,gauge_dest)+6));
	printf("\n* Expires on : %s\n",extract_gauge(gauge->expiry_id ,page_str,gauge_dest));
}

void print_gauge_data(char *gauge_id, char *page_str)
{
	char gauge_dest[256];
	printf("%s\n", extract_gauge(gauge_id, page_str, gauge_dest));
}

/* Connect to keralavision, login and get all data */
int connect_kv(struct App *app ,struct webpage *page){
	int rc =-1;

	if(app->UserName[0] == 0){
		printf("Enter KV Username : ");
		scanf("%s",app->UserName);
		printf("\n");
	}

	rc = get_password(app, MAX_CREDS);
	check_fail(rc == -1,"cannot get Password");

	/* Get tokens for log in  */
	get_webpage(app->base_url, page);
	get_all_tokens(page, app);

	free(page->str);
	/* Log in and get gauge data */
	get_gauge_page(app, page);
	return 0;

}
		
int  main(int argc, char **argv){

	struct webpage page;
	struct App app = {.base_url = "https://selfcare.keralavisionisp.com/Customer/Default.aspx",
					  .login_url = "https://selfcare.keralavisionisp.com/Customer/PortalLogin.aspx?h8=1",
					  .gauge_url = "https://selfcare.keralavisionisp.com/Customer/Gauge.aspx"};

	struct Gauge gauge = {.total_usage_id="lblCurrentUsage",.session_id="lblTotalData",.plan_id="lblPlanName",
						.expiry_id = "lblExpiryDate", .mac_id = "lblMacAddress", .ip_id = "lblFreameIP"};

	const char usage[] = "usage: %s [-h] [-u username]  mode\n";
	const char modes[] = "expiry, plan, mac, ip, session, total_usage";
	const char *about = "kv-cli version v%s\n";

	char gauge_id[32];
	int i= 0;
	/* init username and gauge_id */
	gauge_id[0] = 0;
	app.UserName[0] = 0;

	if(argc >1){
		if(strcmp(argv[1], "-h") == 0 || strcmp(argv[1],"--help") == 0 ){
			printf(about,VERSION);
			printf(usage, argv[0]);
			printf("\tmodes :%s\n",modes);
			exit(EXIT_SUCCESS);
		}
		for(i = 1; i < argc; i++ ){
			if(strcmp(argv[i], "-u") == 0){
				if(++i < argc){
					strncpy(app.UserName, argv[i], MAX_CREDS-1);
				}
				continue;
			}
			if(strcmp(argv[i], "plan") == 0){
				strcpy(gauge_id, gauge.plan_id);
			}
			else if(strcmp(argv[i], "expiry") == 0) {
				strcpy(gauge_id, gauge.expiry_id);
			}
			else if(strcmp(argv[i], "total_usage") == 0) {
				strcpy(gauge_id, gauge.total_usage_id);
			}
			else if(strcmp(argv[i], "session") == 0) {
				strcpy(gauge_id, gauge.session_id);
			}
			else if(strcmp(argv[i], "ip") == 0) {
				strcpy(gauge_id, gauge.ip_id);
			}
			else if(strcmp(argv[i], "mac") == 0) {
				strcpy(gauge_id, gauge.mac_id);
			}
			else if(strcmp(argv[i], "all") == 0) {
				strcpy(gauge_id, "all");
			}
			else{
				check_fail(1,"Unknown argument %s",argv[i]);
			}
		}
	}

	connect_kv(&app, &page);

	if( gauge_id[0] == 0 || strcmp(gauge_id,"all") == 0){
		print_all_gauge_data(&gauge,page.str);
	}
	else{
		print_gauge_data(gauge_id, page.str);
	}

	free(page.str);
	return 0;
}

